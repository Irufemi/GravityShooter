#pragma once

#include <memory>
#include <string>
#include <vector>
#include "Renderer/System/Core/IRenderable.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Type/PrimitiveType.h"
#include "Core/Type/BlendMode.h"
#include "Renderer/Pipeline/PSOManager.h"
#include "Renderer/Object/PrimitiveManager.h"

class ParticleObject;
class Primitive3DObject;

/**
 * @enum EffectType
 * @brief エフェクトの種類を管理する列挙型
 */
enum class EffectType {
    kHit,       // ヒットエフェクト（星型に広がる斬撃など）
    kImpact,    // スライドの表現（PlaneとRingの複合ヒットエフェクト）
    kAura,      // オーラエフェクト
    kSwing,     // スイングエフェクト（風切りエフェクト）
    kExplosion, //!< 3D爆発エフェクト（球体膨張＋パーティクル＋衝撃波）
    // 今後増えるエフェクトの種類をここに追加
};

/**
 * @class Effect
 * @brief 汎用エフェクトクラス
 * @details EffectType を指定することで、適切な初期化・再生を行う
 */
class Effect : public IRenderable {
public:
    Effect();
    ~Effect();

    /**
     * @brief エフェクトの初期化
     * @param camera 使用するカメラ
     * @param type エフェクトの種類
     */
    void Initialize(EffectType type);

    /**
     * @brief エフェクトの更新
     */
    void Update();

    /**
     * @brief エフェクトの描画
     */
    void SyncBeforeDraw() override;
    /**
     * @brief Draw を実行する。
     */
    void Draw() override;

    /**
     * @brief デバッグUIの表示
     * @param name ImGui上で表示するノード名
     */
    void Debug(const char* name = "Effect");
    /**
     * @brief Engine を設定する。
     * @param[in] engine 設定する Engine の値
     */
    static void SetEngine(class IrufemiEngine* engine) {
        engine_ = engine;
    }

    /**
     * @brief 指定した座標にエフェクトを発生させる
     * @param position 発生させるワールド座標
     */
    void Play(const Irufemi::Vector3& position);

    /**
     * @brief 位置・回転・スケールを指定してエフェクトを発生させる（スイングなどの方向固定用）
     * @param position ワールド座標
     * @param rotation 回転角
     * @param scale 追加スケール倍率
     */
    void Play(const Irufemi::Vector3& position, const Irufemi::Vector3& rotation,
              const Irufemi::Vector3& scale = {1.0f, 1.0f, 1.0f});

    /**
     * @brief エフェクトが再生中かどうかを取得する
     * @return 再生中ならtrue
     */
    bool IsActive() const {
        return isActive_;
    }

    struct HitEffectConfig {
        Irufemi::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
        float lifeMin = 1.0f;
        float lifeMax = 1.0f;
        float jitter = 0.0f;
        Irufemi::Vector3 startScaleMin = {0.05f, 0.4f, 1.0f};
        Irufemi::Vector3 startScaleMax = {0.05f, 1.5f, 1.0f};
        Irufemi::Vector3 endScaleMin = {0.05f, 0.0f, 1.0f};
        Irufemi::Vector3 endScaleMax = {0.05f, 0.0f, 1.0f};
        int emitCount = 8;
    };

    struct ImpactConfig {
        Irufemi::PrimitiveType planeShape = Irufemi::PrimitiveType::Plane;
        std::string planeTexture = "resources/circle2.png";
        Irufemi::PrimitiveType ringShape = Irufemi::PrimitiveType::Ring;
        std::string ringTexture = "resources/gradationLine.png";

        Irufemi::Vector2 uvScale = {5.0f, 1.0f};       // RingのU方向スケール
        Irufemi::Vector2 uvScrollSpeed = {1.0f, 0.0f}; // Ringのスクロール速度
        bool useClamp = true;                          // Ringの白丸回避用

        // Plane固有設定
        bool planeEnableRandomRotation = true;
        int planeEmitCount = 4;

        // Ring固有設定
        bool ringEnableRandomRotation = false;
        int ringEmitCount = 1;

        float jitter = 0.0f; // 座標のゆらぎ（時間経過での移動を防ぐため0.0）
        Irufemi::Vector3 planeStartScaleMin = {0.05f, 0.4f, 1.0f};
        Irufemi::Vector3 planeStartScaleMax = {0.05f, 1.5f, 1.0f};
        Irufemi::Vector3 ringStartScaleMin = {0.8f, 0.8f, 1.0f};
        Irufemi::Vector3 ringStartScaleMax = {0.8f, 0.8f, 1.0f};
        Irufemi::Vector3 planeEndScaleMin = {0.05f, 0.0f, 1.0f};
        Irufemi::Vector3 planeEndScaleMax = {0.05f, 0.0f, 1.0f};
        Irufemi::Vector3 ringEndScaleMin = {0.0f, 0.0f, 0.0f};
        Irufemi::Vector3 ringEndScaleMax = {0.0f, 0.0f, 0.0f};
        float lifeMin = 2.0f;
        float lifeMax = 2.0f;
        Irufemi::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    };

    struct AuraConfig {
        Irufemi::Vector3 scale = {1.0f, 1.0f, 1.0f};
        Irufemi::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
        Irufemi::Vector2 uvScrollSpeed = {-0.1f, 0.0f}; // 横にゆっくり流れるように変更
        bool flipV = true;
        bool useClamp = true; // 新規追加したサンプラー(U:Wrap, V:Clamp)が適用されるためtrueをデフォルトに
        std::string texture = "resources/gradationLine.png";
    };

    /** @brief オーラ設定を外部から取得・変更するためのゲッター */
    AuraConfig& GetAuraConfig() {
        return auraConfig_;
    }
    /** @brief オーラ設定を外部から取得するためのゲッター（読み取り専用） */
    const AuraConfig& GetAuraConfig() const {
        return auraConfig_;
    }

    struct SwingConfig {
        Irufemi::PrimitiveType shape = Irufemi::PrimitiveType::Ring; //!< 使用するプリミティブ形状（デフォルト: Ring）
        std::string texture = "resources/gradationLine.png"; //!< 使用するテクスチャパス
        Irufemi::Vector4 color = {1.0f, 0.65f, 0.1f, 1.0f};  //!< スイングエフェクトのデフォルトカラー
        Irufemi::Vector3 startScale = {1.0f, 1.0f, 1.0f}; //!< 開始スケール（半径は呼び出し元で直接指定）
        Irufemi::Vector3 endScale = {1.0f, 1.0f, 1.0f}; //!< 終了スケール
        Irufemi::Vector2 uvScrollSpeed = {0.0f, 0.0f}; //!< 動的メッシュ生成で軌跡が伸びるためスクロールは不要
        Irufemi::Vector2 uvScale = {1.0f, 1.0f}; //!< UVタイリングスケール
        float lifeTime = 0.33f;                  //!< エフェクトの生存時間（約20フレーム = 0.33秒）
        bool useClamp = true;                    //!< 白丸回避用クランプサンプラー使用フラグ

        // リング形状調整用のパラメータ
        float innerRadius = 0.2f;        //!< リングの内径
        float startAngle = 0.0f;         //!< 開始角度
        float endAngle = 140.0f;         //!< 終了角度
        float fadeRangeAngle = 5.0f;     //!< 軌跡のフェード角度
        float swingRotationAngle = 0.0f; //!< エフェクト自体の回転は行わず、レールとして固定
    };

    /**
     * @struct ExplosionConfig
     * @brief 3D爆発エフェクトの設定データ
     */
    struct ExplosionConfig {
        Irufemi::PrimitiveType coreShape =
            Irufemi::PrimitiveType::Sphere;               //!< 3D爆風コアの形状（SphereまたはIcoSphere）
        std::string coreTexture = "resources/noise0.png"; //!< 爆風テクスチャ（既存のノイズを使用）
        Irufemi::PrimitiveType waveShape = Irufemi::PrimitiveType::Ring; //!< 衝撃波の形状
        std::string waveTexture = "resources/gradationLine.png";         //!< 衝撃波のテクスチャ

        Irufemi::Vector4 color = {1.0f, 0.4f, 0.05f, 1.0f};   //!< 燃え上がる高輝度オレンジ
        Irufemi::Vector3 coreStartScale = {0.1f, 0.1f, 0.1f}; //!< 爆風の開始サイズ
        Irufemi::Vector3 coreEndScale = {2.5f, 2.5f, 2.5f}; //!< 爆風の終了サイズ（小さく高密度に修正）
        Irufemi::Vector3 waveStartScale = {0.5f, 0.5f, 0.5f}; //!< 衝撃波の開始サイズ
        Irufemi::Vector3 waveEndScale = {7.0f, 7.0f, 7.0f}; //!< 衝撃波の終了サイズ（スパークの飛散範囲に合わせて調整）

        float lifeTime = 0.4f;                          //!< 爆発の生存時間（秒）
        Irufemi::Vector2 uvScrollSpeed = {0.5f, -0.3f}; //!< コアのうねり用UVスクロール速度
    };

private:
    static class IrufemiEngine* engine_;
    std::unique_ptr<ParticleObject> hitParticle_;
    std::unique_ptr<ParticleObject> impactPlaneParticle_;
    std::unique_ptr<Primitive3DObject> impactRingObject_;
    std::unique_ptr<ParticleObject> explosionSparkParticle_;
    std::unique_ptr<Primitive3DObject> auraObject_;
    std::unique_ptr<Primitive3DObject> swingObject_;         //!< スイング用プリミティブオブジェクト
    std::unique_ptr<Primitive3DObject> explosionObject_;     //!< 3D爆風コア用プリミティブ
    std::unique_ptr<Primitive3DObject> explosionWaveObject_; //!< 衝撃波用プリミティブ
    EffectType type_;

    HitEffectConfig hitConfig_;
    ImpactConfig impactConfig_;
    AuraConfig auraConfig_;
    SwingConfig swingConfig_;         //!< スイング設定パラメータ
    ExplosionConfig explosionConfig_; //!< 3D爆破設定パラメータ
    Irufemi::Vector2 currentUVOffset_ = {0.0f, 0.0f};
    PrimitiveResource customAuraResource_; //!< カスタム炎型オーラのリソース

    // スイング・爆破用生存・制御用変数
    bool isActive_ = false;                              //!< アクティブ状態フラグ
    float lifeTimer_ = 0.0f;                             //!< 残り寿命タイマー
    Irufemi::Vector3 basePosition_ = {0.0f, 0.0f, 0.0f}; //!< 発生位置キャッシュ
    Irufemi::Vector3 baseRotation_ = {0.0f, 0.0f, 0.0f}; //!< 発生回転キャッシュ
    Irufemi::Vector3 baseScale_ = {1.0f, 1.0f, 1.0f};    //!< 発生スケールキャッシュ

    // 全エフェクト共通の設定
    Irufemi::PrimitiveType currentShape_ = Irufemi::PrimitiveType::Plane;
    std::string currentTextureName_ = "resources/circle2.png";

    // 描画設定
    Irufemi::BlendMode blendMode_ = Irufemi::BlendMode::kBlendModeAdd;
    PSOManager::DepthWrite depthWrite_ = PSOManager::DepthWrite::Disable;
    PSOManager::CullMode cullMode_ = PSOManager::CullMode::None;
    bool isBillboard_ = true;
};
