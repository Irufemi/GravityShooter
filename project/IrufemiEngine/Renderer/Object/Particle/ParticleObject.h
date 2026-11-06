#pragma once
#include <string>
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Renderer/System/ParticleGPU/GPUParticleManager.h"
#include "Resource/Model/ModelManager.h"

#include "Core/Type/BlendMode.h"
#include "Core/Utility/JsonUtility.h"

class TextureManager;

/**
 * @class ParticleObject
 * @brief C++べた書きでGPUパーティクルを制御するための描画オブジェクトラッパー
 */
class ParticleObject {
public:
    ParticleObject();
    ~ParticleObject();

    // コピー禁止 (GPUエミッターハンドルの二重解放防止。パラメータ複製は CopyFrom を使用すること)
    ParticleObject(const ParticleObject&) = delete;
    ParticleObject& operator=(const ParticleObject&) = delete;

    // ムーブ操作の明示的許可
    ParticleObject(ParticleObject&&) noexcept = default;
    ParticleObject& operator=(ParticleObject&&) noexcept = default;

    /**
     * @brief Initialize を実行する。
     */
    void Initialize();

    /**
     * @brief GPUバッファの確保を事前に行い、実行中のラグを防ぐ（パラメータは送信しない安全なプレウォーム）
     */
    void PrewarmSystem();

    /**
     * @brief Play を実行する。
     */
    void Play();
    /**
     * @brief Stop を実行する。
     */
    void Stop();
    /**
     * @brief Restart を実行する。
     */
    void Restart();
    /**
     * @brief EmitBurst を実行する。
     */
    void EmitBurst(int count);
    /**
     * @brief Update を実行する。
     */
    void Update();

    /**
     * @brief GPUParticleManager を設定する。
     * @param[in] manager 設定する GPUParticleManager の値
     */
    static void SetGPUParticleManager(GPUParticleManager* manager) {
        gpuParticleManager_ = manager;
    }
    /**
     * @brief ModelManager を設定する。
     * @param[in] manager 設定する ModelManager の値
     */
    static void SetModelManager(ModelManager* manager) {
        modelManager_ = manager;
    }

private:
    inline static GPUParticleManager* gpuParticleManager_ = nullptr;
    inline static ModelManager* modelManager_ = nullptr;

public:
    /**
     * @brief DebugUI を実行する。
     */
    void DebugUI(const char* name = "Particle Object");

    /**
     * @brief Serialize を実行する。
     */
    void Serialize(nlohmann::json& j) const;
    /**
     * @brief Deserialize を実行する。
     */
    void Deserialize(const nlohmann::json& j);
    /**
     * @brief LoadFromJson を実行する。
     */
    bool LoadFromJson(const std::string& filepath);

    /**
     * @brief 他の ParticleObject から全パラメータを直接ディープコピーする
     * @param other コピー元オブジェクト
     */
    void CopyFrom(const ParticleObject& other);

    // --- Getters & Setters ---

    /**
     * @brief パーティクル発生の中心座標を設定し、変更があればGPU転送フラグを立てます
     * @param pos 新しいワールド座標
     */
    void SetPosition(const Irufemi::Vector3& pos) {
        if (position_.x != pos.x || position_.y != pos.y || position_.z != pos.z) {
            position_ = pos;
            MarkDirty();
        }
    }
    /**
     * @brief Position を取得する。
     * @return 取得された Position
     */
    Irufemi::Vector3 GetPosition() const {
        return position_;
    }

    /**
     * @brief Rotation を設定する。
     * @param[in] rot 設定する Rotation の値
     */
    void SetRotation(const Irufemi::Vector3& rot) {
        if (rotation_.x != rot.x || rotation_.y != rot.y || rotation_.z != rot.z) {
            rotation_ = rot;
            MarkDirty();
        }
    }
    /**
     * @brief Rotation を取得する。
     * @return 取得された Rotation
     */
    Irufemi::Vector3 GetRotation() const {
        return rotation_;
    }

    /**
     * @brief TexturePath を設定する。
     * @param[in] path 設定する TexturePath の値
     */
    void SetTexturePath(const std::string& path);
    /**
     * @brief TexturePath を取得する。
     * @return 取得された TexturePath
     */
    std::string GetTexturePath() const {
        return texturePath_;
    }

    /**
     * @brief BlendMode を設定する。
     * @param[in] mode 設定する BlendMode の値
     */
    void SetBlendMode(Irufemi::BlendMode mode);

    /**
     * @brief 深度書き込み（深度テスト）の設定
     */
    void SetDepthWrite(PSOManager::DepthWrite depthWrite);

    /**
     * @brief BlendMode を取得する。
     * @return 取得された BlendMode
     */
    Irufemi::BlendMode GetBlendMode() const {
        return blendMode_;
    }

    /**
     * @brief EnableLighting を設定する。
     * @param[in] val 設定する EnableLighting の値
     */
    void SetEnableLighting(bool val);
    /**
     * @brief EnableLighting を取得する。
     * @return 取得された EnableLighting
     */
    bool GetEnableLighting() const {
        return enableLighting_;
    }

    /**
     * @brief UnscaledTime を設定する。
     * @param[in] val 設定する UnscaledTime の値
     */
    void SetUnscaledTime(bool val);
    /**
     * @brief IsUnscaledTime かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsUnscaledTime() const {
        return isUnscaledTime_;
    }

    /**
     * @brief EmitterModelPath を設定する。
     * @param[in] path 設定する EmitterModelPath の値
     */
    void SetEmitterModelPath(const std::string& path);
    /**
     * @brief EmitterModelPath を取得する。
     * @return 取得された EmitterModelPath
     */
    const std::string& GetEmitterModelPath() const {
        return emitterModelPath_;
    }

    /**
     * @brief EmitOnAwake を設定する。
     * @param[in] val 設定する EmitOnAwake の値
     */
    void SetEmitOnAwake(bool val) {
        emitOnAwake_ = val;
    }
    /**
     * @brief EmitOnAwake を取得する。
     * @return 取得された EmitOnAwake
     */
    bool GetEmitOnAwake() const {
        return emitOnAwake_;
    }

    /**
     * @brief EmitType を設定する。
     * @param[in] type 設定する EmitType の値
     */
    void SetEmitType(int type) {
        if (emitType_ != type) {
            emitType_ = type;
            MarkDirty();
        }
    }
    /**
     * @brief EmitType を取得する。
     * @return 取得された EmitType
     */
    int GetEmitType() const {
        return emitType_;
    }

    /**
     * @brief EmissionRate を設定する。
     * @param[in] rate 設定する EmissionRate の値
     */
    void SetEmissionRate(float rate) {
        if (emissionRate_ != rate) {
            emissionRate_ = rate;
            MarkDirty();
        }
    }
    /**
     * @brief EmissionRate を取得する。
     * @return 取得された EmissionRate
     */
    float GetEmissionRate() const {
        return emissionRate_;
    }

    /**
     * @brief LifeTimeMin を設定する。
     * @param[in] life 設定する LifeTimeMin の値
     */
    void SetLifeTimeMin(float life) {
        if (lifeTimeMin_ != life) {
            lifeTimeMin_ = life;
            MarkDirty();
        }
    }
    /**
     * @brief LifeTimeMin を取得する。
     * @return 取得された LifeTimeMin
     */
    float GetLifeTimeMin() const {
        return lifeTimeMin_;
    }

    /**
     * @brief LifeTimeMax を設定する。
     * @param[in] life 設定する LifeTimeMax の値
     */
    void SetLifeTimeMax(float life) {
        if (lifeTimeMax_ != life) {
            lifeTimeMax_ = life;
            MarkDirty();
        }
    }
    /**
     * @brief LifeTimeMax を取得する。
     * @return 取得された LifeTimeMax
     */
    float GetLifeTimeMax() const {
        return lifeTimeMax_;
    }

    /**
     * @brief Velocity を設定する。
     * @param[in] vel 設定する Velocity の値
     */
    void SetVelocity(float vel) {
        if (velocity_ != vel) {
            velocity_ = vel;
            MarkDirty();
        }
    }
    /**
     * @brief Velocity を取得する。
     * @return 取得された Velocity
     */
    float GetVelocity() const {
        return velocity_;
    }

    /**
     * @brief Radius を設定する。
     * @param[in] rad 設定する Radius の値
     */
    void SetRadius(float rad) {
        if (radius_ != rad) {
            radius_ = rad;
            MarkDirty();
        }
    }
    /**
     * @brief Radius を取得する。
     * @return 取得された Radius
     */
    float GetRadius() const {
        return radius_;
    }

    /**
     * @brief Spread を設定する。
     * @param[in] spr 設定する Spread の値
     */
    void SetSpread(float spr) {
        if (spread_ != spr) {
            spread_ = spr;
            MarkDirty();
        }
    }
    /**
     * @brief Spread を取得する。
     * @return 取得された Spread
     */
    float GetSpread() const {
        return spread_;
    }

    /**
     * @brief AtlasRows を設定する。
     * @param[in] rows 設定する AtlasRows の値
     */
    void SetAtlasRows(int rows) {
        if (atlasRows_ != rows) {
            atlasRows_ = rows;
            MarkDirty();
        }
    }
    /**
     * @brief AtlasRows を取得する。
     * @return 取得された AtlasRows
     */
    int GetAtlasRows() const {
        return atlasRows_;
    }

    /**
     * @brief AtlasCols を設定する。
     * @param[in] cols 設定する AtlasCols の値
     */
    void SetAtlasCols(int cols) {
        if (atlasCols_ != cols) {
            atlasCols_ = cols;
            MarkDirty();
        }
    }
    /**
     * @brief AtlasCols を取得する。
     * @return 取得された AtlasCols
     */
    int GetAtlasCols() const {
        return atlasCols_;
    }

    /**
     * @brief Gravity を設定する。
     * @param[in] grav 設定する Gravity の値
     */
    void SetGravity(float grav) {
        if (gravity_ != grav) {
            gravity_ = grav;
            MarkDirty();
        }
    }
    /**
     * @brief Gravity を取得する。
     * @return 取得された Gravity
     */
    float GetGravity() const {
        return gravity_;
    }

    /**
     * @brief Damping を設定する。
     * @param[in] damp 設定する Damping の値
     */
    void SetDamping(float damp) {
        if (damping_ != damp) {
            damping_ = damp;
            MarkDirty();
        }
    }
    /**
     * @brief Damping を取得する。
     * @return 取得された Damping
     */
    float GetDamping() const {
        return damping_;
    }

    /**
     * @brief Bounce を設定する。
     * @param[in] bounce 設定する Bounce の値
     */
    void SetBounce(float bounce) {
        if (bounce_ != bounce) {
            bounce_ = bounce;
            MarkDirty();
        }
    }
    /**
     * @brief Bounce を取得する。
     * @return 取得された Bounce
     */
    float GetBounce() const {
        return bounce_;
    }

    /**
     * @brief GroundHeight を設定する。
     * @param[in] height 設定する GroundHeight の値
     */
    void SetGroundHeight(float height) {
        if (groundHeight_ != height) {
            groundHeight_ = height;
            MarkDirty();
        }
    }
    /**
     * @brief GroundHeight を取得する。
     * @return 取得された GroundHeight
     */
    float GetGroundHeight() const {
        return groundHeight_;
    }

    /**
     * @brief AttractorStrength を設定する。
     * @param[in] strength 設定する AttractorStrength の値
     */
    void SetAttractorStrength(float strength) {
        if (attractorStrength_ != strength) {
            attractorStrength_ = strength;
            MarkDirty();
        }
    }
    /**
     * @brief AttractorStrength を取得する。
     * @return 取得された AttractorStrength
     */
    float GetAttractorStrength() const {
        return attractorStrength_;
    }

    /**
     * @brief AttractorPos を設定する。
     * @param[in] pos 設定する AttractorPos の値
     */
    void SetAttractorPos(const Irufemi::Vector3& pos) {
        if (attractorPos_.x != pos.x || attractorPos_.y != pos.y || attractorPos_.z != pos.z) {
            attractorPos_ = pos;
            MarkDirty();
        }
    }
    /**
     * @brief AttractorPos を取得する。
     * @return 取得された AttractorPos
     */
    Irufemi::Vector3 GetAttractorPos() const {
        return attractorPos_;
    }

    /**
     * @brief Jitter を設定する。
     * @param[in] jitter 設定する Jitter の値
     */
    void SetJitter(float jitter) {
        if (jitter_ != jitter) {
            jitter_ = jitter;
            MarkDirty();
        }
    }
    /**
     * @brief Jitter を取得する。
     * @return 取得された Jitter
     */
    float GetJitter() const {
        return jitter_;
    }

    /**
     * @brief BillboardMode を設定する。
     * @param[in] mode 設定する BillboardMode の値
     */
    void SetBillboardMode(int mode) {
        if (billboardMode_ != mode) {
            billboardMode_ = mode;
            MarkDirty();
        }
    }
    /**
     * @brief BillboardMode を取得する。
     * @return 取得された BillboardMode
     */
    int GetBillboardMode() const {
        return billboardMode_;
    }

    /**
     * @brief Color を設定する。
     * @param[in] color 設定する Color の値
     */
    void SetColor(const Irufemi::Vector4& color) {
        if (color_.x != color.x || color_.y != color.y || color_.z != color.z || color_.w != color.w) {
            color_ = color;
            MarkDirty();
        }
    }
    /**
     * @brief Color を取得する。
     * @return 取得された Color
     */
    Irufemi::Vector4 GetColor() const {
        return color_;
    }

    /**
     * @brief MidColor を設定する。
     * @param[in] color 設定する MidColor の値
     */
    void SetMidColor(const Irufemi::Vector4& color) {
        if (midColor_.x != color.x || midColor_.y != color.y || midColor_.z != color.z || midColor_.w != color.w) {
            midColor_ = color;
            MarkDirty();
        }
    }
    /**
     * @brief MidColor を取得する。
     * @return 取得された MidColor
     */
    Irufemi::Vector4 GetMidColor() const {
        return midColor_;
    }

    /**
     * @brief StartScale を設定する。
     * @param[in] scale 設定する StartScale の値
     */
    void SetStartScale(const Irufemi::Vector3& scale) {
        if (startScale_.x != scale.x || startScale_.y != scale.y || startScale_.z != scale.z) {
            startScale_ = scale;
            MarkDirty();
        }
    }
    /**
     * @brief StartScale を取得する。
     * @return 取得された StartScale
     */
    Irufemi::Vector3 GetStartScale() const {
        return startScale_;
    }

    /**
     * @brief MidScale を設定する。
     * @param[in] scale 設定する MidScale の値
     */
    void SetMidScale(const Irufemi::Vector3& scale) {
        if (midScale_.x != scale.x || midScale_.y != scale.y || midScale_.z != scale.z) {
            midScale_ = scale;
            MarkDirty();
        }
    }
    /**
     * @brief MidScale を取得する。
     * @return 取得された MidScale
     */
    Irufemi::Vector3 GetMidScale() const {
        return midScale_;
    }

    /**
     * @brief EndScale を設定する。
     * @param[in] scale 設定する EndScale の値
     */
    void SetEndScale(const Irufemi::Vector3& scale) {
        if (endScale_.x != scale.x || endScale_.y != scale.y || endScale_.z != scale.z) {
            endScale_ = scale;
            MarkDirty();
        }
    }
    /**
     * @brief EndScale を取得する。
     * @return 取得された EndScale
     */
    Irufemi::Vector3 GetEndScale() const {
        return endScale_;
    }

    /**
     * @brief MidPoint を設定する。
     * @param[in] point 設定する MidPoint の値
     */
    void SetMidPoint(float point) {
        if (midPoint_ != point) {
            midPoint_ = point;
            MarkDirty();
        }
    }
    /**
     * @brief MidPoint を取得する。
     * @return 取得された MidPoint
     */
    float GetMidPoint() const {
        return midPoint_;
    }

    /**
     * @brief Direction を設定する。
     * @param[in] dir 設定する Direction の値
     */
    void SetDirection(const Irufemi::Vector3& dir) {
        if (direction_.x != dir.x || direction_.y != dir.y || direction_.z != dir.z) {
            direction_ = dir;
            MarkDirty();
        }
    }
    /**
     * @brief Direction を取得する。
     * @return 取得された Direction
     */
    Irufemi::Vector3 GetDirection() const {
        return direction_;
    }

    /**
     * @brief AreaSize を設定する。
     * @param[in] size 設定する AreaSize の値
     */
    void SetAreaSize(const Irufemi::Vector3& size) {
        if (areaSize_.x != size.x || areaSize_.y != size.y || areaSize_.z != size.z) {
            areaSize_ = size;
            MarkDirty();
        }
    }
    /**
     * @brief AreaSize を取得する。
     * @return 取得された AreaSize
     */
    Irufemi::Vector3 GetAreaSize() const {
        return areaSize_;
    }

    /**
     * @brief EnableTrail を設定する。
     * @param[in] enable 設定する EnableTrail の値
     */
    void SetEnableTrail(bool enable) {
        if (enableTrail_ != enable) {
            enableTrail_ = enable;
            MarkDirty();
        }
    }
    /**
     * @brief EnableTrail を取得する。
     * @return 取得された EnableTrail
     */
    bool GetEnableTrail() const {
        return enableTrail_;
    }

    /**
     * @brief TrailFrequency を設定する。
     * @param[in] freq 設定する TrailFrequency の値
     */
    void SetTrailFrequency(float freq) {
        if (trailFrequency_ != freq) {
            trailFrequency_ = freq;
            MarkDirty();
        }
    }
    /**
     * @brief TrailFrequency を取得する。
     * @return 取得された TrailFrequency
     */
    float GetTrailFrequency() const {
        return trailFrequency_;
    }

    /**
     * @brief EnableDeathEmit を設定する。
     * @param[in] enable 設定する EnableDeathEmit の値
     */
    void SetEnableDeathEmit(bool enable) {
        if (enableDeathEmit_ != enable) {
            enableDeathEmit_ = enable;
            MarkDirty();
        }
    }
    /**
     * @brief EnableDeathEmit を取得する。
     * @return 取得された EnableDeathEmit
     */
    bool GetEnableDeathEmit() const {
        return enableDeathEmit_;
    }

    /**
     * @brief EnableRandomRotation を設定する。
     * @param[in] enable 設定する EnableRandomRotation の値
     */
    void SetEnableRandomRotation(bool enable) {
        if (enableRandomRotation_ != enable) {
            enableRandomRotation_ = enable;
            MarkDirty();
        }
    }
    /**
     * @brief EnableRandomRotation を取得する。
     * @return 取得された EnableRandomRotation
     */
    bool GetEnableRandomRotation() const {
        return enableRandomRotation_;
    }

    /**
     * @brief ShowDebugArea を設定する。
     * @param[in] show 設定する ShowDebugArea の値
     */
    void SetShowDebugArea(bool show) {
        if (showDebugArea_ != show) {
            showDebugArea_ = show;
            MarkDirty();
        }
    }
    /**
     * @brief ShowDebugArea を取得する。
     * @return 取得された ShowDebugArea
     */
    bool GetShowDebugArea() const {
        return showDebugArea_;
    }

    /**
     * @brief TextureManager を設定する。
     * @param[in] tm 設定する TextureManager の値
     */
    static void SetTextureManager(TextureManager* tm) {
        textureManager_ = tm;
    }
    /**
     * @brief TextureManager を取得する。
     * @return 取得された TextureManager
     */
    static TextureManager* GetTextureManager() {
        return textureManager_;
    }

    /**
     * @brief RegisterProperties を実行する。
     */
    void RegisterProperties(class Component* comp);

    /**
     * @brief MarkDirty を実行する。
     */
    void MarkDirty() {
        isDirty_ = true;
    }

private:
    // トランスフォーム
    Irufemi::Vector3 position_ = {0.0f, 0.0f, 0.0f};
    Irufemi::Vector3 rotation_ = {0.0f, 0.0f, 0.0f}; // 現在は主に方向として使用

    std::string texturePath_ = "resources/circle.png";
    Irufemi::BlendMode blendMode_ = Irufemi::BlendMode::kBlendModeAdd;
    PSOManager::DepthWrite depthWrite_ = PSOManager::DepthWrite::Disable;
    bool enableLighting_ = false;
    bool isUnscaledTime_ = false;
    bool emitOnAwake_ = true;
    int burstCountOnAwake_ = 0;

    std::string emitterModelPath_;
    ResourceHandle emitterModelHandle_;

    // エミッターの基本パラメータ
    int emitType_ = 0;           // 0: Irufemi::Sphere, 1: Beam, 2: Box, 3: Irufemi::Cylinder
    float emissionRate_ = 50.0f; // 1秒あたりの発生数
    float lifeTimeMin_ = 0.5f;
    float lifeTimeMax_ = 1.0f;
    float velocity_ = 1.0f;
    float radius_ = 0.0f;
    float spread_ = 0.1f;

    // アニメーション設定
    int atlasRows_ = 1;
    int atlasCols_ = 1;

    // 物理・挙動パラメータ
    float gravity_ = 0.0f;
    float damping_ = 0.0f;
    float bounce_ = 0.0f;                                // 床でのバウンド係数
    float groundHeight_ = -100.0f;                       // 床のY座標
    float attractorStrength_ = 0.0f;                     // 吸引力
    Irufemi::Vector3 attractorPos_ = {0.0f, 0.0f, 0.0f}; // 吸引位置
    float jitter_ = 0.0f;                                // 不規則なブレ
    bool enableTrail_ = false;
    float trailFrequency_ = 0.05f;
    bool enableDeathEmit_ = false;
    bool enableRandomRotation_ = false;
    bool showDebugArea_ = true; //!< デバッグエリア表示フラグ

    // ビジュアル・ライフタイム
    int billboardMode_ = 1; // 0: None, 1: Billboard, 2: Y-Axis
    Irufemi::Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f};
    Irufemi::Vector4 midColor_ = {1.0f, 1.0f, 1.0f, 1.0f}; // 中間色
    Irufemi::Vector3 startScale_ = {1.0f, 1.0f, 1.0f};
    Irufemi::Vector3 midScale_ = {1.0f, 1.0f, 1.0f}; // 中間スケール
    Irufemi::Vector3 endScale_ = {0.0f, 0.0f, 0.0f};
    float midPoint_ = 0.5f; // 中間点の位置(0.0~1.0)

    Irufemi::Vector3 direction_ = {0.0f, 0.0f, 0.0f};
    Irufemi::Vector3 areaSize_ = {10.0f, 10.0f, 10.0f}; // Boxエミッター用サイズ

private:
    /**
     * @brief UpdateSystem を実行する。
     */
    void UpdateSystem();

    static TextureManager* textureManager_;
    GPUParticleManager::EmitterHandle emitterHandle_;
    bool isPlaying_ = false;
    int burstCountPending_ = 0;
    bool isDirty_ = true; // パラメータ変更検知フラグ
};
