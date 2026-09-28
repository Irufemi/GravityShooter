#include "Player/PlayerTargetingComponent.h"
#include "Player/TargetableComponent.h"
#include "UI/LockonMarkerUIComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"
#include "Physics/CollisionManager.h"
#include "Core/Math/MathFunction.h"
#include "Core/Shape/LinePrimitive.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include <algorithm>
#include <limits>
#include <cmath>
#include <unordered_set>

PlayerTargetingComponent::~PlayerTargetingComponent() {
    RestoreAllOutlineHighlights();
}

void PlayerTargetingComponent::Initialize() {
    TryFindLockonMarkerUI();
}

void PlayerTargetingComponent::Start() {
    TryFindLockonMarkerUI();
}

void PlayerTargetingComponent::TryFindLockonMarkerUI() {
    if (!lockonMarkerUI_.expired() || !gameObject_) {
        return;
    }
    if (auto scene = gameObject_->GetScene()) {
        for (const auto& obj : scene->GetGameObjects()) {
            if (auto ui = obj->GetComponent<LockonMarkerUIComponent>()) {
                lockonMarkerUI_ = ui->weak_from_this();
                break;
            }
        }
    }
}

void PlayerTargetingComponent::Update() {
    // 死んだオブジェクトやターゲット不可になったオブジェクトをキューから削除する (C++20 std::erase_if)
    std::erase_if(queuedTargets_, [this](const std::shared_ptr<GameObject>& obj) {
        if (!obj || !obj->GetIsActive() || obj->IsDestroyed()) {
            return true;
        }

        // TargetableComponent による共通ターゲット可否判定
        if (auto targetable = obj->GetComponent<TargetableComponent>()) {
            return !targetable->IsTargetable() || !IsTargetTypeAllowed(targetable->GetTargetType());
        }

        return true;
    });

    UpdateHoverTarget();

    if (lockonMarkerUI_.expired()) {
        float dt = GetEngine() ? GetEngine()->GetGameDeltaTime() : (1.0f / 60.0f);
        uiSearchTimer_ += dt;
        if (uiSearchTimer_ >= kUISearchInterval) {
            uiSearchTimer_ = 0.0f;
            TryFindLockonMarkerUI();
        }
    } else {
        uiSearchTimer_ = 0.0f;
    }

    if (auto markerUI = lockonMarkerUI_.lock()) {
        markerUI->SetMaxLockonCount(maxLockonCount_);
        std::vector<std::shared_ptr<GameObject>> displayTargets(queuedTargets_.begin(), queuedTargets_.end());
        if (hoverTarget_) {
            displayTargets.push_back(hoverTarget_);
        }
        markerUI->SyncTargets(displayTargets);
    }

    // ロックオン・ホバー対象のアウトラインハイライトを同期更新 (Juice)
    UpdateOutlineHighlights();
}

void PlayerTargetingComponent::OnRegisterProperties() {}

void PlayerTargetingComponent::UpdateHoverTarget() {
    hoverTarget_ = nullptr;

    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    auto cameraManager = engine->GetCameraManager();
    if (!cameraManager || !cameraManager->GetActiveCamera()) {
        return;
    }
    auto camera = cameraManager->GetActiveCamera();

    Irufemi::Matrix4x4 viewProj = camera->GetViewProjectionMatrix3D();
    float viewWidth = camera->GetViewportWidth();
    float viewHeight = camera->GetViewportHeight();

    auto inputManager = engine->GetInputManager();
    Irufemi::Vector2 screenCenter =
        inputManager ? inputManager->GetMousePosition() : Irufemi::Vector2{viewWidth * 0.5f, viewHeight * 0.5f};
    float currentTime = engine->GetTotalTime();

    // 1. 保留中の非同期レイキャストをポーリングして視線キャッシュを更新
    for (auto it = visibilityCache_.begin(); it != visibilityCache_.end();) {
        auto targetObj = it->second.targetObject.lock();
        TargetVisibilityCache& cache = it->second;

        // オブジェクトが破棄されていたらキャッシュから安全に削除 (UAF防止)
        if (!targetObj || !targetObj->GetIsActive() || targetObj->IsDestroyed()) {
            it = visibilityCache_.erase(it);
            continue;
        }

        if (cache.pendingTask) {
            if (cache.pendingTask->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                auto result = cache.pendingTask->get();
                bool hit = result.first;
                RaycastHit hitInfo = result.second;

                bool canSee = true;
                auto transform = targetObj->GetComponent<TransformComponent>();
                if (transform) {
                    Irufemi::Vector3 targetPos = transform->GetWorldPosition();
                    Irufemi::Vector3 cameraPos = camera->GetTranslate();
                    float dist3D = Irufemi::Math::Length(Irufemi::Math::Subtract(targetPos, cameraPos));

                    if (hit && hitInfo.hitObject != nullptr) {
                        if (hitInfo.hitObject != targetObj.get() && hitInfo.distance < dist3D - 1.0f) {
                            canSee = false; // 障害物に遮蔽されている
                        }
                    }
                }
                cache.canSee = canSee;
                cache.hasCheckedOnce = true;
                cache.pendingTask.reset();
            }
        }
        ++it;
    }

    std::shared_ptr<GameObject> bestTarget = nullptr;
    float bestScore = (std::numeric_limits<float>::max)();

    auto scene = gameObject_->GetScene();
    if (!scene) {
        return;
    }
    auto playerObj = gameObject_;

    // 2. ターゲット候補のスコアリングと評価
    for (auto targetComp : TargetableComponent::GetTargets()) {
        if (!targetComp || !targetComp->IsTargetable() || !IsTargetTypeAllowed(targetComp->GetTargetType())) {
            continue;
        }

        auto obj = targetComp->GetGameObject();
        if (!obj || !obj->GetIsActive() || obj->IsDestroyed()) {
            continue;
        }

        auto transform = obj->GetComponent<TransformComponent>();
        if (transform) {

            Irufemi::Vector3 worldPos = transform->GetWorldPosition();
            Irufemi::Vector3 clipPos = Irufemi::Math::Transform(worldPos, viewProj);

            if (clipPos.z >= 0.0f && clipPos.z <= 1.0f) {
                float screenX = (clipPos.x + 1.0f) * 0.5f * viewWidth;
                float screenY = (1.0f - clipPos.y) * 0.5f * viewHeight;

                float dx = screenX - screenCenter.x;
                float dy = screenY - screenCenter.y;
                float dist2DSq = dx * dx + dy * dy;

                if (dist2DSq <= lockonRadius2D_ * lockonRadius2D_) {
                    Irufemi::Vector3 cameraPos = camera->GetTranslate();
                    Irufemi::Vector3 toTarget = Irufemi::Math::Subtract(worldPos, cameraPos);
                    float dist3D = Irufemi::Math::Length(toTarget);

                    float score = std::sqrt(dist2DSq) * weight2D_ + dist3D * weight3D_;

                    if (score < bestScore) {
                        auto& cache = visibilityCache_[obj->GetInstanceID()];
                        cache.targetObject = obj->shared_from_this();

                        if (!cache.hasCheckedOnce) {
                            // 初回は同期Raycastで遮蔽を即座に確定し、壁裏敵の一瞬の透過ロックオンを防止
                            Irufemi::Vector3 dir = Irufemi::Math::Normalize(toTarget);
                            Irufemi::Ray ray;
                            ray.origin = cameraPos;
                            ray.diff = dir;
                            RaycastHit hitInfo{};
                            bool hit = engine->GetCollisionManager()->Raycast(ray, hitInfo, dist3D + 10.0f, 0xFFFFFFFF,
                                                                              playerObj);

                            bool canSee = true;
                            if (hit && hitInfo.hitObject != nullptr) {
                                if (hitInfo.hitObject != obj && hitInfo.distance < dist3D - 1.0f) {
                                    canSee = false;
                                }
                            }
                            cache.canSee = canSee;
                            cache.hasCheckedOnce = true;
                            cache.lastCheckTime = currentTime;
                        } else {
                            // 2回目以降: 0.1秒以上経過していれば、非同期レイキャストを発行（Amortization）
                            if (currentTime - cache.lastCheckTime > 0.1f && !cache.pendingTask) {
                                cache.lastCheckTime = currentTime;
                                Irufemi::Vector3 dir = Irufemi::Math::Normalize(toTarget);
                                Irufemi::Ray ray;
                                ray.origin = cameraPos;
                                ray.diff = dir;

                                cache.pendingTask = std::make_shared<std::future<std::pair<bool, RaycastHit>>>(
                                    engine->GetCollisionManager()->RaycastAsync(engine->GetThreadPool(), ray,
                                                                                dist3D + 10.0f, 0xFFFFFFFF, playerObj));
                            }
                        }

                        // 視認可能な場合のみベストターゲット候補とする
                        if (cache.canSee) {
                            bestScore = score;
                            bestTarget = obj->shared_from_this();
                        }
                    }
                }
            }
        }
    }

    hoverTarget_ = bestTarget;
}

void PlayerTargetingComponent::MarkTarget(size_t maxLockOn) {
    if (queuedTargets_.size() >= maxLockOn) {
        return;
    }

    if (hoverTarget_) {
        queuedTargets_.push_back(hoverTarget_);
    }
}

void PlayerTargetingComponent::ClearTargets() {
    RestoreAllOutlineHighlights();
    queuedTargets_.clear();
}

std::shared_ptr<GameObject> PlayerTargetingComponent::PopTarget() {
    if (queuedTargets_.empty()) {
        return nullptr;
    }
    auto target = std::move(queuedTargets_.front());
    queuedTargets_.pop_front();
    UpdateOutlineHighlights();
    return target;
}

Irufemi::Vector3 PlayerTargetingComponent::CalculateAimPoint(float maxDistance) const {
    auto engine = GetEngine();
    if (!engine) {
        return {0.0f, 0.0f, 0.0f};
    }

    auto cameraManager = engine->GetCameraManager();
    auto inputManager = engine->GetInputManager();
    if (!cameraManager || !cameraManager->GetActiveCamera() || !inputManager) {
        return {0.0f, 0.0f, 0.0f};
    }

    auto camera = cameraManager->GetActiveCamera();
    float width = camera->GetViewportWidth();
    float height = camera->GetViewportHeight();
    Irufemi::Vector2 mousePos = inputManager->GetMousePosition();

    Irufemi::Matrix4x4 viewProjInv = Irufemi::Math::Inverse(camera->GetViewProjectionMatrix3D());
    Irufemi::Ray ray = Irufemi::Math::ScreenPointToRay(mousePos, width, height, viewProjInv);

    RaycastHit hitInfo;
    if (auto collisionManager = engine->GetCollisionManager()) {
        if (collisionManager->Raycast(ray, hitInfo, maxDistance)) {
            return hitInfo.hitPoint;
        }
    }

    return Irufemi::Math::Add(ray.origin, Irufemi::Math::Multiply(maxDistance, ray.diff));
}

void PlayerTargetingComponent::UpdateOutlineHighlights() {
    std::unordered_set<uint64_t> currentTargetIds;

    // 現在キューに入っているロックオン対象
    for (const auto& target : queuedTargets_) {
        if (target && target->GetIsActive() && !target->IsDestroyed()) {
            currentTargetIds.insert(target->GetInstanceID());
        }
    }
    // 現在レティクルがホバーしている対象
    if (hoverTarget_ && hoverTarget_->GetIsActive() && !hoverTarget_->IsDestroyed()) {
        currentTargetIds.insert(hoverTarget_->GetInstanceID());
    }

    auto scene = gameObject_ ? gameObject_->GetScene() : nullptr;

    // 1. 今回ハイライトから外れたオブジェクトを元の色に戻す
    for (auto it = originalOutlineColors_.begin(); it != originalOutlineColors_.end();) {
        uint64_t id = it->first;
        if (currentTargetIds.find(id) == currentTargetIds.end()) {
            if (scene) {
                if (auto obj = scene->FindGameObjectByID(id)) {
                    if (auto maskComp = obj->GetComponent<EffectMaskComponent>()) {
                        auto params = maskComp->GetCustomParams();
                        params.color1 = it->second;
                        maskComp->SetCustomParams(params);
                    }
                }
            }
            it = originalOutlineColors_.erase(it);
        } else {
            ++it;
        }
    }

    // 2. 現在のターゲットを警告ロックオンイエローにハイライト
    const Irufemi::Vector4 lockonYellow = {1.0f, 0.9f, 0.1f, 1.0f};
    auto applyHighlight = [&](const std::shared_ptr<GameObject>& obj) {
        if (!obj || !obj->GetIsActive() || obj->IsDestroyed()) {
            return;
        }
        if (auto maskComp = obj->GetComponent<EffectMaskComponent>()) {
            uint64_t id = obj->GetInstanceID();
            if (originalOutlineColors_.find(id) == originalOutlineColors_.end()) {
                originalOutlineColors_[id] = maskComp->GetCustomParams().color1;
            }
            auto params = maskComp->GetCustomParams();
            params.color1 = lockonYellow;
            maskComp->SetCustomParams(params);
        }
    };

    for (const auto& target : queuedTargets_) {
        applyHighlight(target);
    }
    if (hoverTarget_) {
        applyHighlight(hoverTarget_);
    }
}

void PlayerTargetingComponent::RestoreAllOutlineHighlights() {
    auto scene = gameObject_ ? gameObject_->GetScene() : nullptr;
    if (scene) {
        for (const auto& [id, color] : originalOutlineColors_) {
            if (auto obj = scene->FindGameObjectByID(id)) {
                if (auto maskComp = obj->GetComponent<EffectMaskComponent>()) {
                    auto params = maskComp->GetCustomParams();
                    params.color1 = color;
                    maskComp->SetCustomParams(params);
                }
            }
        }
    }
    originalOutlineColors_.clear();
}
