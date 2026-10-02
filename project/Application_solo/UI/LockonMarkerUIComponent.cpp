#include "UI/LockonMarkerUIComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/Pipeline/PSOManager.h"
#include "Core/Utility/Ease.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"
#include "Core/Math/MathFunction.h"
#include "Framework/UI/DebugUI.h"
#include "Core/Utility/ErrorUtility.h"
#define NOMINMAX
#include <windows.h>
#include <string>
#include <cmath>
#include <algorithm>
#include <unordered_map>

void LockonMarkerUIComponent::Initialize() {
    auto engine = GetEngine();
    if (!engine) {
        return; // シーンへのエンジンバインド完了まで初期化を遅延
    }

    if (!markerBatch_) {
        // 1. SpriteBatch の生成（テクスチャ指定）
        markerBatch_ = std::make_unique<SpriteBatch>();
        markerBatch_->Initialize("resources/reticle.jpg");

        // 2. カスタムシェーダー（輝度アルファ抜き）の適用 (名前指定によるホットリロード安全化)
        markerBatch_->SetCustomPSO("LuminanceAlpha2D", Irufemi::BlendMode::kBlendModeNormal,
                                   PSOManager::DepthWrite::Off, PSOManager::CullMode::None);
    }
}

void LockonMarkerUIComponent::SyncTargets(const std::vector<std::shared_ptr<GameObject>>& targets) {
    nextMarkersCache_.clear();
    targetCountsCache_.clear();

    for (const auto& target : targets) {
        if (!target) {
            continue;
        }

        int occurrenceIndex = targetCountsCache_[target->GetInstanceID()]++;

        bool found = false;
        int activeOccurrence = 0;
        for (const auto& active : activeMarkers_) {
            if (active.target.lock() == target) {
                if (activeOccurrence == occurrenceIndex) {
                    nextMarkersCache_.push_back(active);
                    found = true;
                    break;
                }
                activeOccurrence++;
            }
        }

        if (!found) {
            // 新規ロックオン対象
            LockonMarkerState newState;
            newState.target = target;
            newState.currentScale = 3.0f; // 初期スケール（大きめに出現）
            newState.animationT = 0.0f;
            nextMarkersCache_.push_back(newState);
        }
    }

    activeMarkers_ = nextMarkersCache_;
}

void LockonMarkerUIComponent::Update() {
    if (!markerBatch_) {
        Initialize();
        if (!markerBatch_) {
            return;
        }
    }

    // バッチへの登録をリセット
    markerBatch_->ClearInstances();

    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    float deltaTime = engine->GetDeltaTime();

    auto cameraManager = engine->GetCameraManager();
    IRUFEMI_ASSERT(cameraManager != nullptr && "cameraManager is null");
    auto camera = cameraManager->GetActiveCamera();
    IRUFEMI_ASSERT(camera != nullptr && "camera is null");

    int markerIndex = 0;
    drawCountsCache_.clear();

    for (auto& marker : activeMarkers_) {
        auto target = marker.target.lock();
        if (!target) {
            continue;
        }

        auto transform = target->GetComponent<TransformComponent>();
        if (!transform) {
            continue;
        }

        // 3D座標から2Dスクリーン座標への手動変換（Zは0.0～1.0の深度）
        Irufemi::Vector3 worldPos = transform->GetWorldPosition();
        Irufemi::Matrix4x4 viewProj = camera->GetViewProjectionMatrix3D();
        Irufemi::Vector3 clipPos = Irufemi::Math::Transform(worldPos, viewProj);

        Irufemi::Vector3 screenPos;
        screenPos.z = clipPos.z;
        screenPos.x = (clipPos.x + 1.0f) * 0.5f * camera->GetViewportWidth();
        screenPos.y = (1.0f - clipPos.y) * 0.5f * camera->GetViewportHeight();

        Irufemi::Vector2 uiPos = camera->ScreenToUIPosition({screenPos.x, screenPos.y});
        screenPos.x = uiPos.x;
        screenPos.y = uiPos.y;

        // 画面奥に行っている場合はスキップ（Z > 1.0 または Z < 0.0）
        if (screenPos.z >= 1.0f || screenPos.z <= 0.0f) {

            continue;
        }

        // 距離に応じた基本スケールの計算（遠近法: 基準距離50.0fとして反比例）
        Irufemi::Vector3 cameraPos = camera->GetTranslate();
        float dist3D = Irufemi::Math::Length(Irufemi::Math::Subtract(worldPos, cameraPos));
        float distanceScale = std::clamp(50.0f / (std::max)(dist3D, 1.0f), 0.3f, 1.2f);
        marker.targetScale = distanceScale;

        // アニメーション（イージング）の進行
        if (marker.animationT < 1.0f) {
            marker.animationT += deltaTime * 5.0f; // アニメーション速度 (約0.2秒で完了)
            if (marker.animationT > 1.0f) {
                marker.animationT = 1.0f;
            }
        }

        // EaseOutCubic を使ってシュッと縮小するアニメーション
        float easedT = EaseOutCubic(marker.animationT);
        marker.currentScale = std::lerp(3.0f, marker.targetScale, easedT);

        int idx = drawCountsCache_[target->GetInstanceID()]++;
        float finalScale = marker.currentScale;
        Irufemi::Vector2 finalPos = {screenPos.x, screenPos.y};

        // 回転と色の設定
        float maxLocks = (std::max)(1.0f, static_cast<float>(maxLockonCount_));
        float angleStep = 6.283185f / maxLocks;
        float rotation = 0.0f;

        // ターゲットを重ねるごとに G と B 成分を減らし、赤みを強くする (White -> Orange -> Red)
        float intensity = std::clamp(1.0f - (idx * 0.25f), 0.1f, 1.0f);
        Irufemi::Vector4 color = {1.0f, intensity, intensity, 1.0f};

        if (idx > 0) {
            // 同心円（スタック）方式：重なるごとに少しずつ小さくする
            finalScale *= (1.0f - idx * 0.15f);

            // 上限数で分割した角度ずつずらして回転させる
            float rotationSpeed = (idx % 2 == 0) ? 2.0f : -2.0f;
            rotation = idx * angleStep + (engine->GetGameTime() * rotationSpeed);
        } else {
            // ベースのマーカーもゆっくり回転
            rotation = engine->GetGameTime() * 1.5f;
        }

        // バッチにインスタンスを追加 (SpriteBatchのAddInstanceはVector2, Irufemi::Vector2, rotation, color, anchor)
        float size = 64.0f * finalScale;
        markerBatch_->AddInstance(finalPos, Irufemi::Vector2{size, size}, rotation, color);

        markerIndex++;
    }

    // インスタンスバッファを構築（これを呼ばないとGPUにデータが送られない）
    markerBatch_->Update();

    // 描画キューにバッチを登録する（これを呼ばないと描画されない）
    markerBatch_->Draw();
}
