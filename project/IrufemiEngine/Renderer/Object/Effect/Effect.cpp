#include "Renderer/Object/Effect/Effect.h"
#include "Renderer/Object/Particle/ParticleObject.h"
#include "Renderer/System/ParticleGPU/GPUParticleSystem.h"
#include "Framework/UI/DebugUI.h"
#include "Resource/Texture/TextureManager.h"
#include "Core/Math/Math.h"
#include "Engine/Irufemi.h"
#include "Renderer/Object/3D/Primitive/Primitive3DObject.h"
#include "Renderer/Object/PrimitiveManager.h"

IrufemiEngine* Effect::engine_ = nullptr;

Effect::Effect() = default;
Effect::~Effect() = default;

void Effect::Initialize(EffectType type) {
    type_ = type;

    switch (type_) {
    case EffectType::kHit: {
        isBillboard_ = true;

        hitParticle_ = std::make_unique<ParticleObject>();
        hitParticle_->SetTexturePath(currentTextureName_);
        hitParticle_->SetBlendMode(blendMode_);
        hitParticle_->SetBillboardMode(1); // Billboard mode
        hitParticle_->SetColor(hitConfig_.color);
        hitParticle_->SetLifeTimeMin(hitConfig_.lifeMin);
        hitParticle_->SetLifeTimeMax(hitConfig_.lifeMax);
        hitParticle_->SetStartScale(hitConfig_.startScaleMax);
        hitParticle_->SetEndScale(hitConfig_.endScaleMax);
        hitParticle_->SetJitter(hitConfig_.jitter);
        hitParticle_->SetEmitOnAwake(false);
        hitParticle_->Initialize();
        break;
    }
    case EffectType::kImpact: {
        isBillboard_ = false;

        // Planeエミッター (破片) -> ParticleObject
        impactPlaneParticle_ = std::make_unique<ParticleObject>();
        impactPlaneParticle_->SetTexturePath(impactConfig_.planeTexture);
        impactPlaneParticle_->SetBlendMode(blendMode_);
        impactPlaneParticle_->SetBillboardMode(0); // None
        impactPlaneParticle_->SetColor(impactConfig_.color);
        impactPlaneParticle_->SetLifeTimeMin(impactConfig_.lifeMin);
        impactPlaneParticle_->SetLifeTimeMax(impactConfig_.lifeMax);
        impactPlaneParticle_->SetStartScale(impactConfig_.planeStartScaleMax);
        impactPlaneParticle_->SetEndScale(impactConfig_.planeEndScaleMax);
        impactPlaneParticle_->SetJitter(impactConfig_.jitter);
        impactPlaneParticle_->SetEmitOnAwake(false);
        impactPlaneParticle_->Initialize();

        // Ringエミッター -> Primitive3DObject
        impactRingObject_ = std::make_unique<Primitive3DObject>();
        impactRingObject_->Initialize(impactConfig_.ringShape, impactConfig_.ringTexture);
        impactRingObject_->SetCastShadows(false);
        impactRingObject_->GetMaterial().enableLighting = false;
        impactRingObject_->GetMaterial().color = impactConfig_.color;
        impactRingObject_->GetMaterial().useClampSampler = impactConfig_.useClamp ? 3 : 0;
        impactRingObject_->SetScale(impactConfig_.ringStartScaleMax);
        isActive_ = false;
        break;
    }
    case EffectType::kAura: {
        isBillboard_ = false;
        auraObject_ = std::make_unique<Primitive3DObject>();
        auraObject_->Initialize(Irufemi::PrimitiveType::Cylinder, auraConfig_.texture);

        // 炎のように立ち上る形状（底面半径0.6f, 上面半径0.05f, 高さ2.5f, 底面原点）を動的生成して適用
        // 上面をほぼ尖らせることで炎の先端のシルエットを表現。セグメント24で滑らかに。
        PrimitiveData customAuraData = PrimitiveManager::CreateCylinder(0.6f, 0.05f, 2.5f, 24, false, false, false);
        engine_->GetPrimitiveManager()->CreateGPUResource(customAuraData, customAuraResource_);

        if (auraObject_->GetMesh().resource) {
            auraObject_->GetMesh().resource->SetVertexBufferView(customAuraResource_.vertexBufferView);
            auraObject_->GetMesh().resource->SetIndexBufferView(customAuraResource_.indexBufferView);
            auraObject_->GetMesh().resource->SetIndexCount(customAuraResource_.indexCount);
        }

        auraObject_->SetCastShadows(false);                // エフェクトなので影は不要
        auraObject_->GetMaterial().enableLighting = false; // ライティング不要
        auraObject_->GetMaterial().color = auraConfig_.color;
        auraObject_->GetMaterial().useClampSampler = auraConfig_.useClamp ? 3 : 0; // 3 = U:Wrap, V:Clamp
        auraObject_->SetScale(auraConfig_.scale);                                  // 初期スケールの適用
        break;
    }
    case EffectType::kSwing: {
        isBillboard_ = false;
        swingObject_ = std::make_unique<Primitive3DObject>();
        swingObject_->Initialize(Irufemi::PrimitiveType::Ring, swingConfig_.texture);

        // 風切りスイングに特化したカスタム形状パラメータ（端が尖った半円）
        RingParams ringParams;
        ringParams.innerRadius = swingConfig_.innerRadius;
        ringParams.startOuterRadius = 1.0f;
        ringParams.endOuterRadius = 1.0f;
        ringParams.startAngle = swingConfig_.startAngle;
        ringParams.endAngle = swingConfig_.endAngle;
        ringParams.segments = 32;
        ringParams.fadeRangeAngle = swingConfig_.fadeRangeAngle;
        ringParams.verticalUV = false;
        ringParams.innerColor = {1.0f, 1.0f, 1.0f, 1.0f};
        ringParams.outerColor = {1.0f, 1.0f, 1.0f, 1.0f};
        ringParams.startAlpha = 0.0f;
        ringParams.endAlpha = 0.0f;

        PrimitiveData ringData = engine_->GetPrimitiveManager()->CreateRing(ringParams);

        // 頂点座標を XZ平面に寝かせる（ハンマーの軌道に沿ったレールにする）
        for (auto& vertex : ringData.vertices) {
            float tempY = vertex.position.y;
            vertex.position.y = 0.0f;  // Y軸を潰してXZ平面に寝かせる
            vertex.position.z = tempY; // 元のY(上下)をZ(前後)に変換

            float tempNy = vertex.normal.y;
            vertex.normal.y = 0.0f;
            vertex.normal.z = tempNy;
        }

        swingObject_->ReinitializeMesh(ringData);

        swingObject_->SetCastShadows(false);                // エフェクトなので影は不要
        swingObject_->GetMaterial().enableLighting = false; // ライティング不要
        swingObject_->GetMaterial().color = swingConfig_.color;
        swingObject_->GetMaterial().useClampSampler = swingConfig_.useClamp ? 3 : 0; // 3 = U:Wrap, V:Clamp
        swingObject_->SetScale(swingConfig_.startScale);
        isActive_ = false;
        break;
    }
    case EffectType::kExplosion: {
        isBillboard_ = false;

        // 1. 3D球体コア
        explosionObject_ = std::make_unique<Primitive3DObject>();
        explosionObject_->Initialize(explosionConfig_.coreShape, explosionConfig_.coreTexture);
        explosionObject_->SetCastShadows(false);
        explosionObject_->GetMaterial().enableLighting = false;
        explosionObject_->GetMaterial().color = explosionConfig_.color;
        explosionObject_->SetScale(explosionConfig_.coreStartScale);

        // 2. 3軸衝撃波リング
        explosionWaveObject_ = std::make_unique<Primitive3DObject>();
        explosionWaveObject_->Initialize(explosionConfig_.waveShape, explosionConfig_.waveTexture);
        explosionWaveObject_->SetCastShadows(false);
        explosionWaveObject_->GetMaterial().enableLighting = false;
        explosionWaveObject_->GetMaterial().color = explosionConfig_.color;
        explosionWaveObject_->SetScale(explosionConfig_.waveStartScale);

        // 3. GPUパーティクル（火花用）
        explosionSparkParticle_ = std::make_unique<ParticleObject>();
        explosionSparkParticle_->SetTexturePath("resources/circle2.png");
        explosionSparkParticle_->SetBlendMode(Irufemi::BlendMode::kBlendModeAdd);
        explosionSparkParticle_->SetBillboardMode(1); // CameraBillboard

        // 元の SparkBehavior（白 -> 赤 -> 透明）をシミュレート
        explosionSparkParticle_->SetColor({1.0f, 1.0f, 0.8f, 1.0f});
        explosionSparkParticle_->SetMidColor({1.0f, 0.6f, 0.0f, 1.0f});
        explosionSparkParticle_->SetMidPoint(0.2f);

        explosionSparkParticle_->SetStartScale({0.08f, 0.08f, 0.08f});
        explosionSparkParticle_->SetMidScale({0.08f, 0.08f, 0.08f});
        explosionSparkParticle_->SetEndScale({0.0f, 0.0f, 0.0f});

        explosionSparkParticle_->SetLifeTimeMin(0.3f);
        explosionSparkParticle_->SetLifeTimeMax(0.6f);
        explosionSparkParticle_->SetGravity(0.8f);
        explosionSparkParticle_->SetDamping(0.05f);
        explosionSparkParticle_->SetVelocity(0.0f); // 爆風とは別でPlay時に設定する
        explosionSparkParticle_->SetEmitOnAwake(false);
        explosionSparkParticle_->Initialize();

        isActive_ = false;
        break;
    }
    }
}

void Effect::Update() {
    float dt = engine_->GetDeltaTime();

    if (type_ == EffectType::kImpact && impactRingObject_ && isActive_) {
        lifeTimer_ -= dt;
        if (lifeTimer_ <= 0.0f) {
            isActive_ = false;
        } else {
            float t = 1.0f - (lifeTimer_ / impactConfig_.lifeMax);

            Irufemi::Vector3 ringScale;
            ringScale.x = Lerp(impactConfig_.ringStartScaleMax.x, impactConfig_.ringEndScaleMax.x, t) * baseScale_.x;
            ringScale.y = Lerp(impactConfig_.ringStartScaleMax.y, impactConfig_.ringEndScaleMax.y, t) * baseScale_.y;
            ringScale.z = Lerp(impactConfig_.ringStartScaleMax.z, impactConfig_.ringEndScaleMax.z, t) * baseScale_.z;
            impactRingObject_->SetScale(ringScale);

            Irufemi::Vector4 ringColor = impactConfig_.color;
            ringColor.w = Lerp(impactConfig_.color.w, 0.0f, t);
            impactRingObject_->GetMaterial().color = ringColor;

            currentUVOffset_.x += impactConfig_.uvScrollSpeed.x * dt;
            currentUVOffset_.y += impactConfig_.uvScrollSpeed.y * dt;

            Irufemi::Vector3 uvScale = {impactConfig_.uvScale.x, impactConfig_.uvScale.y, 1.0f};
            Irufemi::Vector3 uvRot = {0.0f, 0.0f, 0.0f};
            Irufemi::Vector3 uvTrans = {currentUVOffset_.x, currentUVOffset_.y, 0.0f};
            impactRingObject_->GetMaterial().uvTransform = Irufemi::Math::MakeAffineMatrix(uvScale, uvRot, uvTrans);

            // Z-fightingを避けるためにPlay時にずらした座標を維持
            impactRingObject_->Update();
        }
    } else if (type_ == EffectType::kAura && auraObject_) {
        // スクロール量の加算
        currentUVOffset_.x += auraConfig_.uvScrollSpeed.x * dt;
        currentUVOffset_.y += auraConfig_.uvScrollSpeed.y * dt;

        // flipVが有効な場合はスケールYを反転し、オフセットをずらす
        float scaleY = auraConfig_.flipV ? -1.0f : 1.0f;
        float offsetY = auraConfig_.flipV ? currentUVOffset_.y + 1.0f : currentUVOffset_.y;

        Irufemi::Vector3 scale = {1.0f, scaleY, 1.0f};
        Irufemi::Vector3 rot = {0.0f, 0.0f, 0.0f};
        Irufemi::Vector3 trans = {currentUVOffset_.x, offsetY, 0.0f};

        // 行列を構築し、MaterialComponentのuvTransformに流し込む
        auraObject_->GetMaterial().uvTransform = Irufemi::Math::MakeAffineMatrix(scale, rot, trans);
        auraObject_->GetMaterial().color = auraConfig_.color;
        auraObject_->GetMaterial().useClampSampler = auraConfig_.useClamp ? 3 : 0; // 3 = U:Wrap, V:Clamp
        if (auraObject_->GetMaterial().texturePath != auraConfig_.texture) {
            auraObject_->SetTexture(auraConfig_.texture);
        }

        auraObject_->Update();
    } else if (type_ == EffectType::kSwing && swingObject_ && isActive_) {
        lifeTimer_ -= dt;
        if (lifeTimer_ <= 0.0f) {
            isActive_ = false;
        } else {
            // 生存割合の計算 (0.0f -> 1.0f)
            float t = 1.0f - (lifeTimer_ / swingConfig_.lifeTime);

            // スケールを startScale から endScale へ線形補間
            Irufemi::Vector3 currentScale;
            currentScale.x = Lerp(swingConfig_.startScale.x, swingConfig_.endScale.x, t) * baseScale_.x;
            currentScale.y = Lerp(swingConfig_.startScale.y, swingConfig_.endScale.y, t) * baseScale_.y;
            currentScale.z = Lerp(swingConfig_.startScale.z, swingConfig_.endScale.z, t) * baseScale_.z;
            swingObject_->SetScale(currentScale);

            // アルファ値（透明度）をフェードアウト
            Irufemi::Vector4 currentColor = swingConfig_.color;
            currentColor.w = Lerp(swingConfig_.color.w, 0.0f, t);
            swingObject_->GetMaterial().color = currentColor;

            // 動的にメッシュを再生成し「武器の先端から弧が伸びていく」かつ「残像が消えていく」アニメーションを実装
            RingParams ringParams;
            ringParams.innerRadius = swingConfig_.innerRadius;
            ringParams.startOuterRadius = 1.0f;
            ringParams.endOuterRadius = 1.0f;

            // tの進行に合わせて弧を伸ばす。先端が武器を追い越さないよう、進行度tと完全に等速(1.0倍)にする
            float currentEndAngle = Lerp(0.1f, swingConfig_.endAngle, t);
            float currentStartAngle = 0.0f;

            // スイング後半から根本が消えていく。t=1.0の時点でも少し軌跡が残るように0.8倍で止める
            if (t > 0.5f) {
                float fadeT = std::clamp((t - 0.5f) * 2.0f, 0.0f, 1.0f);
                currentStartAngle = Lerp(0.0f, swingConfig_.endAngle * 0.8f, fadeT);
            }

            // 先端と根本が逆転しないように最低限の幅を確保
            if (currentEndAngle <= currentStartAngle + 1.0f) {
                currentEndAngle = currentStartAngle + 1.0f;
            }

            ringParams.startAngle = currentStartAngle;
            ringParams.endAngle = currentEndAngle;

            ringParams.segments = 32;
            ringParams.fadeRangeAngle = swingConfig_.fadeRangeAngle;
            ringParams.verticalUV = false;
            ringParams.innerColor = {1.0f, 1.0f, 1.0f, 1.0f};
            ringParams.outerColor = {1.0f, 1.0f, 1.0f, 1.0f};
            ringParams.startAlpha = 0.0f;
            ringParams.endAlpha = 0.0f;

            PrimitiveData ringData = PrimitiveManager::CreateRing(ringParams);

            // 頂点座標を XZ平面に寝かせる
            for (auto& vertex : ringData.vertices) {
                float tempY = vertex.position.y;
                vertex.position.y = 0.0f;
                vertex.position.z = tempY;
                float tempNy = vertex.normal.y;
                vertex.normal.y = 0.0f;
                vertex.normal.z = tempNy;
            }
            swingObject_->ReinitializeMesh(ringData);

            // UVスクロールの更新
            currentUVOffset_.x += swingConfig_.uvScrollSpeed.x * dt;
            currentUVOffset_.y += swingConfig_.uvScrollSpeed.y * dt;

            Irufemi::Vector3 uvScale = {swingConfig_.uvScale.x, swingConfig_.uvScale.y, 1.0f};
            Irufemi::Vector3 uvRot = {0.0f, 0.0f, 0.0f};
            Irufemi::Vector3 uvTrans = {currentUVOffset_.x, currentUVOffset_.y, 0.0f};
            swingObject_->GetMaterial().uvTransform = Irufemi::Math::MakeAffineMatrix(uvScale, uvRot, uvTrans);
            swingObject_->GetMaterial().useClampSampler = swingConfig_.useClamp ? 3 : 0;

            if (swingObject_->GetMaterial().texturePath != swingConfig_.texture) {
                swingObject_->SetTexture(swingConfig_.texture);
            }

            // 最新の位置と回転を反映
            Irufemi::Vector3 currentRotation = baseRotation_;
            currentRotation.y -= swingConfig_.swingRotationAngle * t;

            swingObject_->SetPosition(basePosition_);
            swingObject_->SetRotate(currentRotation);
            swingObject_->Update();
        }
    } else if (type_ == EffectType::kExplosion && explosionObject_ && isActive_) {
        lifeTimer_ -= dt;
        if (lifeTimer_ <= 0.0f) {
            isActive_ = false;
        } else {
            // 生存割合 (0.0f -> 1.0f)
            float t = 1.0f - (lifeTimer_ / explosionConfig_.lifeTime);

            // イージング（急激に膨張して減速するイージングアウト）
            float easeOut = 1.0f - std::pow(1.0f - t, 3.0f); // Cubic Ease Out

            // 1. 3Dコア球体の更新
            Irufemi::Vector3 coreScale;
            coreScale.x =
                Lerp(explosionConfig_.coreStartScale.x, explosionConfig_.coreEndScale.x, easeOut) * baseScale_.x;
            coreScale.y =
                Lerp(explosionConfig_.coreStartScale.y, explosionConfig_.coreEndScale.y, easeOut) * baseScale_.y;
            coreScale.z =
                Lerp(explosionConfig_.coreStartScale.z, explosionConfig_.coreEndScale.z, easeOut) * baseScale_.z;
            explosionObject_->SetScale(coreScale);

            Irufemi::Vector4 coreColor = explosionConfig_.color;
            coreColor.w = Lerp(explosionConfig_.color.w, 0.0f, t); // 徐々に透明に
            explosionObject_->GetMaterial().color = coreColor;

            // コアのUVスクロールで炎のうねりを表現
            currentUVOffset_.x += explosionConfig_.uvScrollSpeed.x * dt;
            currentUVOffset_.y += explosionConfig_.uvScrollSpeed.y * dt;
            explosionObject_->GetMaterial().uvTransform =
                Irufemi::Math::MakeAffineMatrix(Irufemi::Vector3{1.0f, 1.0f, 1.0f}, Irufemi::Vector3{0.0f, 0.0f, 0.0f},
                                                Irufemi::Vector3{currentUVOffset_.x, currentUVOffset_.y, 0.0f});

            explosionObject_->SetPosition(basePosition_);
            explosionObject_->Update();

            // 2. 衝撃波リングの更新 (スケールが極端に小さい場合は更新を最小限に)
            if (explosionWaveObject_) {
                bool isSmallExplosion = (baseScale_.x < 0.5f);
                Irufemi::Vector3 waveScale;
                waveScale.x =
                    Lerp(explosionConfig_.waveStartScale.x, explosionConfig_.waveEndScale.x, easeOut) * baseScale_.x;
                waveScale.y =
                    Lerp(explosionConfig_.waveStartScale.y, explosionConfig_.waveEndScale.y, easeOut) * baseScale_.y;
                waveScale.z =
                    Lerp(explosionConfig_.waveStartScale.z, explosionConfig_.waveEndScale.z, easeOut) * baseScale_.z;
                explosionWaveObject_->SetScale(waveScale);

                Irufemi::Vector4 waveColor = explosionConfig_.color;
                waveColor.w = Lerp(explosionConfig_.color.w, 0.0f, t);
                explosionWaveObject_->GetMaterial().color = waveColor;

                explosionWaveObject_->SetPosition(basePosition_);

                // 小規模爆発の時は、毎フレームのUpdate回数を減らすため、ここでの1回のみUpdateを行う
                if (isSmallExplosion) {
                    explosionWaveObject_->SetRotate({0.0f, 0.0f, 0.0f});
                    explosionWaveObject_->Update();
                }
            }
        }
    }

    if (hitParticle_) {
        hitParticle_->Update();
    }
    if (impactPlaneParticle_) {
        impactPlaneParticle_->Update();
    }
    if (explosionSparkParticle_) {
        explosionSparkParticle_->Update();
    }
}

void Effect::SyncBeforeDraw() {
    if (type_ == EffectType::kImpact && impactRingObject_ && isActive_) {
        impactRingObject_->SyncBeforeDraw();
    }
    if (type_ == EffectType::kAura && auraObject_) {
        auraObject_->SyncBeforeDraw();
    }
    if (type_ == EffectType::kSwing && swingObject_ && isActive_) {
        swingObject_->SyncBeforeDraw();
    }
    if (type_ == EffectType::kExplosion && isActive_) {
        bool isSmallExplosion = (baseScale_.x < 0.5f);
        if (explosionObject_) {
            explosionObject_->SyncBeforeDraw();
        }
        if (explosionWaveObject_) {
            // 小規模爆発の場合は1枚しか描画しないため、ここで1回だけ同期する
            if (isSmallExplosion) {
                explosionWaveObject_->SyncBeforeDraw();
            }
        }
    }
}

void Effect::Draw() {
    if (type_ == EffectType::kImpact && impactRingObject_ && isActive_) {
        auto* engine = GPUParticleSystem::GetEngine();
        Irufemi::BlendMode prevBlend = engine->GetBlend();
        PSOManager::DepthWrite prevDepth = engine->GetDepthWrite();
        PSOManager::CullMode prevCull = engine->GetCull();

        engine->SetBlend(blendMode_);
        engine->SetDepthWrite(depthWrite_);
        engine->SetCull(cullMode_);

        impactRingObject_->Draw();

        engine->SetBlend(prevBlend);
        engine->SetDepthWrite(prevDepth);
        engine->SetCull(prevCull);
    }
    if (type_ == EffectType::kAura && auraObject_) {
        auto* engine = GPUParticleSystem::GetEngine();

        // 現在のステートを退避
        Irufemi::BlendMode prevBlend = engine->GetBlend();
        PSOManager::DepthWrite prevDepth = engine->GetDepthWrite();
        PSOManager::CullMode prevCull = engine->GetCull();

        // エフェクト用のステートを設定
        engine->SetBlend(blendMode_);
        engine->SetDepthWrite(depthWrite_);
        engine->SetCull(cullMode_);

        // 描画
        auraObject_->Draw();

        // ステートを元に戻す
        engine->SetBlend(prevBlend);
        engine->SetDepthWrite(prevDepth);
        engine->SetCull(prevCull);
    }
    if (type_ == EffectType::kSwing && swingObject_ && isActive_) {
        auto* engine = GPUParticleSystem::GetEngine();

        // 現在のステートを退避
        Irufemi::BlendMode prevBlend = engine->GetBlend();
        PSOManager::DepthWrite prevDepth = engine->GetDepthWrite();
        PSOManager::CullMode prevCull = engine->GetCull();

        // エフェクト用のステートを設定
        engine->SetBlend(blendMode_);
        engine->SetDepthWrite(depthWrite_);
        engine->SetCull(cullMode_);

        // 平面メッシュに「縦軸の厚み」を持たせるため、Y座標を少しずつズラして3枚（ミルフィーユ状）描画する
        Irufemi::Vector3 originalPos = basePosition_;

        // 1枚目（上端）
        Irufemi::Vector3 topPos = originalPos;
        topPos.y += 0.6f;
        swingObject_->SetPosition(topPos);
        swingObject_->Update();
        swingObject_->SyncBeforeDraw();
        swingObject_->Draw();

        // 2枚目（中央）
        swingObject_->SetPosition(originalPos);
        swingObject_->Update();
        swingObject_->SyncBeforeDraw();
        swingObject_->Draw();

        // 3枚目（下端）
        Irufemi::Vector3 bottomPos = originalPos;
        bottomPos.y -= 0.6f;
        swingObject_->SetPosition(bottomPos);
        swingObject_->Update();
        swingObject_->SyncBeforeDraw();
        swingObject_->Draw();

        // 位置を元に戻しておく
        swingObject_->SetPosition(originalPos);

        // ステートを元に戻す
        engine->SetBlend(prevBlend);
        engine->SetDepthWrite(prevDepth);
        engine->SetCull(prevCull);
    }
    if (type_ == EffectType::kExplosion && isActive_) {
        auto* engine = GPUParticleSystem::GetEngine();

        // 現在のステートを退避
        Irufemi::BlendMode prevBlend = engine->GetBlend();
        PSOManager::DepthWrite prevDepth = engine->GetDepthWrite();
        PSOManager::CullMode prevCull = engine->GetCull();

        // エフェクト用のステートを設定（加算、デプス書き込み無効、カリングなし）
        engine->SetBlend(Irufemi::BlendMode::kBlendModeAdd);
        engine->SetDepthWrite(PSOManager::DepthWrite::Disable);
        engine->SetCull(PSOManager::CullMode::None);

        // 1. 3D球体コアを描画
        if (explosionObject_) {
            explosionObject_->Draw();
        }

        // 2. 3軸衝撃波リングを描画（スケールに応じて最適化）
        if (explosionWaveObject_) {
            bool isSmallExplosion = (baseScale_.x < 0.5f);

            // 水平（XZ平面）- マシンガン等の小規模爆発時も必ず描画
            if (!isSmallExplosion) {
                explosionWaveObject_->SetRotate({0.0f, 0.0f, 0.0f});
                explosionWaveObject_->Update();
                explosionWaveObject_->SyncBeforeDraw();
            }
            explosionWaveObject_->Draw();

            // スケールが大きい（ミサイル等）時のみ垂直リング（XY,
            // YZ平面）を描画し、描画コールと同期処理を大幅にカット！
            if (!isSmallExplosion) {
                // 垂直（XY平面 - ピッチ回転）
                explosionWaveObject_->SetRotate({1.57f, 0.0f, 0.0f});
                explosionWaveObject_->Update();
                explosionWaveObject_->SyncBeforeDraw();
                explosionWaveObject_->Draw();

                // 垂直（YZ平面 - ロール回転）
                explosionWaveObject_->SetRotate({0.0f, 0.0f, 1.57f});
                explosionWaveObject_->Update();
                explosionWaveObject_->SyncBeforeDraw();
                explosionWaveObject_->Draw();
            }
        }

        // ステートを元に戻す
        engine->SetBlend(prevBlend);
        engine->SetDepthWrite(prevDepth);
        engine->SetCull(prevCull);
    }
}

void Effect::Debug(const char* name) {
#if defined(USE_IMGUI)
    if (ImGui::Begin(name)) {
        if (ImGui::BeginTabBar("EffectParamsTabs")) {

            // --- 共通設定タブ ---
            if (ImGui::BeginTabItem("Common Settings")) {
                const char* typeNames[] = {"Hit", "Impact", "Aura", "Swing", "Explosion"};
                int currentType = static_cast<int>(type_);
                if (ImGui::Combo("Effect Type", &currentType, typeNames, IM_ARRAYSIZE(typeNames))) {
                    if (engine_) {
                        Initialize(static_cast<EffectType>(currentType));
                    }
                }

                ImGui::Separator();
                bool pipelineChanged = false;
                Irufemi::BlendMode prevBlend = blendMode_;
                PSOManager::DepthWrite prevDepth = depthWrite_;
                PSOManager::CullMode prevCull = cullMode_;
                DebugUI::DebugPsoSettings(&blendMode_, &depthWrite_, &cullMode_, "##EffectPso");
                if (prevBlend != blendMode_ || prevDepth != depthWrite_ || prevCull != cullMode_) {
                    pipelineChanged = true;
                }

                if (ImGui::Checkbox("Use Billboard", &isBillboard_)) {
                    pipelineChanged = true;
                }

                if (pipelineChanged && engine_) {
                    Initialize(type_);
                }

                ImGui::EndTabItem();
            }

            // --- 固有設定タブ ---
            if (type_ == EffectType::kHit) {
                if (ImGui::BeginTabItem("Hit Specific Config")) {
                    bool changed = false;
                    const char* primitiveShapeNames[] = {"Triangle", "Plane",  "Cube", "Cylinder", "Sphere",
                                                         "Tetra",    "Circle", "Ring", "Skybox"};
                    int currentShape = static_cast<int>(currentShape_);
                    if (ImGui::Combo("Primitive Shape", &currentShape, primitiveShapeNames,
                                     IM_ARRAYSIZE(primitiveShapeNames))) {
                        currentShape_ = static_cast<Irufemi::PrimitiveType>(currentShape);
                        changed = true;
                    }

                    if (auto* tm = engine_->GetTextureManager()) {
                        auto textureNames = tm->GetTextureNamesForDebug();
                        if (ImGui::BeginCombo("Texture", currentTextureName_.c_str())) {
                            for (size_t i = 0; i < textureNames.size(); i++) {
                                bool is_selected = (currentTextureName_ == textureNames[i]);
                                if (ImGui::Selectable(textureNames[i].c_str(), is_selected)) {
                                    currentTextureName_ = textureNames[i];
                                    changed = true;
                                }
                                if (is_selected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    ImGui::Separator();
                    ImGui::Text("--- Hit Emitter Parameters ---");
                    if (ImGui::ColorEdit4("Color", &hitConfig_.color.x)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat2("Life (Min/Max)", &hitConfig_.lifeMin, 0.01f, 0.01f, 10.0f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat("Jitter", &hitConfig_.jitter, 0.001f, 0.0f, 1.0f)) {
                        changed = true;
                    }
                    if (ImGui::DragInt("Emit Count", &hitConfig_.emitCount, 1, 1, 500)) {
                        changed = true;
                    }

                    ImGui::Separator();
                    ImGui::Text("Scale Parameters");
                    if (ImGui::DragFloat3("Start Scale Min", &hitConfig_.startScaleMin.x, 0.01f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Start Scale Max", &hitConfig_.startScaleMax.x, 0.01f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("End Scale Min", &hitConfig_.endScaleMin.x, 0.01f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("End Scale Max", &hitConfig_.endScaleMax.x, 0.01f)) {
                        changed = true;
                    }

                    if (changed && engine_) {
                        Initialize(EffectType::kHit);
                    }
                    ImGui::EndTabItem();
                }
            } else if (type_ == EffectType::kImpact) {
                if (ImGui::BeginTabItem("Impact Specific Config")) {
                    bool changed = false;

                    ImGui::Text("--- Plane Emitter ---");
                    const char* primitiveShapeNames[] = {"Triangle", "Plane",  "Cube", "Cylinder", "Sphere",
                                                         "Tetra",    "Circle", "Ring", "Skybox"};
                    int currentPlaneShape = static_cast<int>(impactConfig_.planeShape);
                    if (ImGui::Combo("Plane Shape", &currentPlaneShape, primitiveShapeNames,
                                     IM_ARRAYSIZE(primitiveShapeNames))) {
                        impactConfig_.planeShape = static_cast<Irufemi::PrimitiveType>(currentPlaneShape);
                        changed = true;
                    }
                    if (auto* tm = engine_->GetTextureManager()) {
                        auto textureNames = tm->GetTextureNamesForDebug();
                        if (ImGui::BeginCombo("Plane Texture", impactConfig_.planeTexture.c_str())) {
                            for (size_t i = 0; i < textureNames.size(); i++) {
                                bool is_selected = (impactConfig_.planeTexture == textureNames[i]);
                                if (ImGui::Selectable(textureNames[i].c_str(), is_selected)) {
                                    impactConfig_.planeTexture = textureNames[i];
                                    changed = true;
                                }
                                if (is_selected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    if (ImGui::Checkbox("Enable Random 3D Rotation##Plane", &impactConfig_.planeEnableRandomRotation)) {
                        changed = true;
                    }
                    if (ImGui::DragInt("Emit Count##Plane", &impactConfig_.planeEmitCount, 1, 1, 50)) {
                        changed = true;
                    }

                    ImGui::Separator();
                    ImGui::Text("--- Ring Emitter ---");
                    int currentRingShape = static_cast<int>(impactConfig_.ringShape);
                    if (ImGui::Combo("Ring Shape", &currentRingShape, primitiveShapeNames,
                                     IM_ARRAYSIZE(primitiveShapeNames))) {
                        impactConfig_.ringShape = static_cast<Irufemi::PrimitiveType>(currentRingShape);
                        changed = true;
                    }
                    if (auto* tm = engine_->GetTextureManager()) {
                        auto textureNames = tm->GetTextureNamesForDebug();
                        if (ImGui::BeginCombo("Ring Texture", impactConfig_.ringTexture.c_str())) {
                            for (size_t i = 0; i < textureNames.size(); i++) {
                                bool is_selected = (impactConfig_.ringTexture == textureNames[i]);
                                if (ImGui::Selectable(textureNames[i].c_str(), is_selected)) {
                                    impactConfig_.ringTexture = textureNames[i];
                                    changed = true;
                                }
                                if (is_selected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }
                    if (ImGui::Checkbox("Enable Random 3D Rotation##Ring", &impactConfig_.ringEnableRandomRotation)) {
                        changed = true;
                    }
                    if (ImGui::DragInt("Emit Count##Ring", &impactConfig_.ringEmitCount, 1, 1, 50)) {
                        changed = true;
                    }

                    ImGui::Separator();

                    ImGui::Text("--- Transform / Physics ---");
                    if (ImGui::DragFloat("Jitter (Random Walk)", &impactConfig_.jitter, 0.001f, 0.0f, 1.0f)) {
                        changed = true;
                    }

                    if (ImGui::DragFloat2("UV Scale (Ring)", &impactConfig_.uvScale.x, 0.1f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat2("UV Scroll Speed (Ring)", &impactConfig_.uvScrollSpeed.x, 0.1f)) {
                        changed = true;
                    }
                    if (ImGui::Checkbox("Use Clamp Sampler (Ring)", &impactConfig_.useClamp)) {
                        changed = true;
                    }
                    if (ImGui::ColorEdit4("Color", &impactConfig_.color.x)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat2("Life (Min/Max)", &impactConfig_.lifeMin, 0.01f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Plane Start Scale Min", &impactConfig_.planeStartScaleMin.x, 0.01f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Plane Start Scale Max", &impactConfig_.planeStartScaleMax.x, 0.01f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Plane End Scale Min", &impactConfig_.planeEndScaleMin.x, 0.01f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Plane End Scale Max", &impactConfig_.planeEndScaleMax.x, 0.01f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Ring Start Scale Min", &impactConfig_.ringStartScaleMin.x, 0.01f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Ring Start Scale Max", &impactConfig_.ringStartScaleMax.x, 0.01f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Ring End Scale Min", &impactConfig_.ringEndScaleMin.x, 0.01f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Ring End Scale Max", &impactConfig_.ringEndScaleMax.x, 0.01f)) {
                        changed = true;
                    }

                    if (changed && engine_) {
                        Initialize(EffectType::kImpact);
                    }
                    ImGui::EndTabItem();
                }
            } else if (type_ == EffectType::kAura) {
                if (ImGui::BeginTabItem("Aura Specific Config")) {
                    bool changed = false;

                    if (auto* tm = engine_->GetTextureManager()) {
                        auto textureNames = tm->GetTextureNamesForDebug();
                        if (ImGui::BeginCombo("Aura Texture", auraConfig_.texture.c_str())) {
                            for (size_t i = 0; i < textureNames.size(); i++) {
                                bool is_selected = (auraConfig_.texture == textureNames[i]);
                                if (ImGui::Selectable(textureNames[i].c_str(), is_selected)) {
                                    auraConfig_.texture = textureNames[i];
                                    changed = true;
                                }
                                if (is_selected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    if (ImGui::ColorEdit4("Color", &auraConfig_.color.x)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Scale (Radius X/Z, Height Y)", &auraConfig_.scale.x, 0.1f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat2("UV Scroll Speed", &auraConfig_.uvScrollSpeed.x, 0.1f)) {
                        changed = true;
                    }
                    if (ImGui::Checkbox("Flip V", &auraConfig_.flipV)) {
                        changed = true;
                    }
                    if (ImGui::Checkbox("Use Clamp", &auraConfig_.useClamp)) {
                        changed = true;
                    }

                    if (changed && auraObject_) {
                        auraObject_->SetScale(auraConfig_.scale);
                    }

                    ImGui::EndTabItem();
                }
            } else if (type_ == EffectType::kSwing) {
                if (ImGui::BeginTabItem("Swing Specific Config")) {
                    bool changed = false;

                    const char* primitiveShapeNames[] = {"Triangle", "Plane",  "Cube", "Cylinder", "Sphere",
                                                         "Tetra",    "Circle", "Ring", "Skybox"};
                    int currentShape = static_cast<int>(swingConfig_.shape);
                    if (ImGui::Combo("Shape", &currentShape, primitiveShapeNames, IM_ARRAYSIZE(primitiveShapeNames))) {
                        swingConfig_.shape = static_cast<Irufemi::PrimitiveType>(currentShape);
                        changed = true;
                    }

                    if (auto* tm = GPUParticleSystem::GetTextureManager()) {
                        auto textureNames = tm->GetTextureNamesForDebug();
                        if (ImGui::BeginCombo("Texture", swingConfig_.texture.c_str())) {
                            for (size_t i = 0; i < textureNames.size(); i++) {
                                bool is_selected = (swingConfig_.texture == textureNames[i]);
                                if (ImGui::Selectable(textureNames[i].c_str(), is_selected)) {
                                    swingConfig_.texture = textureNames[i];
                                    changed = true;
                                }
                                if (is_selected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    if (ImGui::ColorEdit4("Color", &swingConfig_.color.x)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Start Scale", &swingConfig_.startScale.x, 0.1f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("End Scale", &swingConfig_.endScale.x, 0.1f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat2("UV Scroll Speed", &swingConfig_.uvScrollSpeed.x, 0.1f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat2("UV Scale", &swingConfig_.uvScale.x, 0.1f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat("Life Time", &swingConfig_.lifeTime, 0.01f, 0.01f, 5.0f)) {
                        changed = true;
                    }
                    if (ImGui::Checkbox("Use Clamp", &swingConfig_.useClamp)) {
                        changed = true;
                    }

                    // 新規追加したカスタムパラメータのUI
                    ImGui::Separator();
                    ImGui::Text("Ring Shape Settings");
                    if (ImGui::DragFloat("Inner Radius", &swingConfig_.innerRadius, 0.01f, 0.0f, 1.0f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat("Start Angle", &swingConfig_.startAngle, 1.0f, 0.0f, 360.0f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat("End Angle", &swingConfig_.endAngle, 1.0f, 0.0f, 360.0f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat("Fade Range Angle", &swingConfig_.fadeRangeAngle, 1.0f, 0.0f, 180.0f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat("Swing Rotation Angle (Rad)", &swingConfig_.swingRotationAngle, 0.01f, 0.0f,
                                         6.28f)) {
                        changed = true;
                    }

                    if (changed && swingObject_) {
                        Initialize(EffectType::kSwing);
                    }

                    ImGui::EndTabItem();
                }
            } else if (type_ == EffectType::kExplosion) {
                if (ImGui::BeginTabItem("Explosion Specific Config")) {
                    bool changed = false;

                    ImGui::Text("--- 3D Core Sphere Settings ---");
                    const char* primitiveShapeNames[] = {"Triangle", "Plane",       "Cube",     "Cylinder",
                                                         "Sphere",   "Tetra",       "Circle",   "Ring",
                                                         "Skybox",   "CylinderCap", "IcoSphere"};
                    int currentShape = static_cast<int>(explosionConfig_.coreShape);
                    if (ImGui::Combo("Core Shape", &currentShape, primitiveShapeNames,
                                     IM_ARRAYSIZE(primitiveShapeNames))) {
                        explosionConfig_.coreShape = static_cast<Irufemi::PrimitiveType>(currentShape);
                        changed = true;
                    }

                    if (auto* tm = engine_->GetTextureManager()) {
                        auto textureNames = tm->GetTextureNamesForDebug();
                        if (ImGui::BeginCombo("Core Texture", explosionConfig_.coreTexture.c_str())) {
                            for (size_t i = 0; i < textureNames.size(); i++) {
                                bool is_selected = (explosionConfig_.coreTexture == textureNames[i]);
                                if (ImGui::Selectable(textureNames[i].c_str(), is_selected)) {
                                    explosionConfig_.coreTexture = textureNames[i];
                                    changed = true;
                                }
                                if (is_selected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    ImGui::Separator();
                    ImGui::Text("--- Explosion Wave Settings ---");
                    int currentWaveShape = static_cast<int>(explosionConfig_.waveShape);
                    if (ImGui::Combo("Wave Shape", &currentWaveShape, primitiveShapeNames,
                                     IM_ARRAYSIZE(primitiveShapeNames))) {
                        explosionConfig_.waveShape = static_cast<Irufemi::PrimitiveType>(currentWaveShape);
                        changed = true;
                    }

                    if (auto* tm = engine_->GetTextureManager()) {
                        auto textureNames = tm->GetTextureNamesForDebug();
                        if (ImGui::BeginCombo("Wave Texture", explosionConfig_.waveTexture.c_str())) {
                            for (size_t i = 0; i < textureNames.size(); i++) {
                                bool is_selected = (explosionConfig_.waveTexture == textureNames[i]);
                                if (ImGui::Selectable(textureNames[i].c_str(), is_selected)) {
                                    explosionConfig_.waveTexture = textureNames[i];
                                    changed = true;
                                }
                                if (is_selected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    ImGui::Separator();
                    ImGui::Text("--- Explosion Parameters ---");
                    if (ImGui::ColorEdit4("Color", &explosionConfig_.color.x)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Core Start Scale", &explosionConfig_.coreStartScale.x, 0.05f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Core End Scale", &explosionConfig_.coreEndScale.x, 0.05f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Wave Start Scale", &explosionConfig_.waveStartScale.x, 0.05f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat3("Wave End Scale", &explosionConfig_.waveEndScale.x, 0.05f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat("Life Time", &explosionConfig_.lifeTime, 0.01f, 0.05f, 5.0f)) {
                        changed = true;
                    }
                    if (ImGui::DragFloat2("UV Scroll Speed (Core)", &explosionConfig_.uvScrollSpeed.x, 0.05f)) {
                        changed = true;
                    }

                    if (changed) {
                        Initialize(EffectType::kExplosion);
                    }

                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }

        ImGui::Separator();
        if (ImGui::Button("Play Effect")) {
            Play({0.0f, 0.0f, 0.0f});
        }
    }
    ImGui::End();
#endif
}

void Effect::Play(const Irufemi::Vector3& position) {
    Play(position, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
}

void Effect::Play(const Irufemi::Vector3& position, const Irufemi::Vector3& rotation, const Irufemi::Vector3& scale) {
    basePosition_ = position;
    baseRotation_ = rotation;
    baseScale_ = scale;

    switch (type_) {
    case EffectType::kHit:
        if (hitParticle_) {
            hitParticle_->SetPosition(position);
            hitParticle_->SetVelocity(0.0f);
            hitParticle_->SetRadius(0.0f);
            hitParticle_->MarkDirty();
            hitParticle_->EmitBurst(hitConfig_.emitCount);
        }
        break;
    case EffectType::kImpact:
        if (impactPlaneParticle_) {
            impactPlaneParticle_->SetPosition(position);
            impactPlaneParticle_->SetVelocity(0.0f);
            impactPlaneParticle_->SetRadius(0.0f);
            impactPlaneParticle_->MarkDirty();
            impactPlaneParticle_->EmitBurst(impactConfig_.planeEmitCount);
        }
        if (impactRingObject_) {
            isActive_ = true;
            lifeTimer_ = impactConfig_.lifeMax;
            currentUVOffset_ = {0.0f, 0.0f};
            Irufemi::Vector3 ringPos = position;
            ringPos.y += 0.001f;
            impactRingObject_->SetPosition(ringPos);
            impactRingObject_->SetScale(impactConfig_.ringStartScaleMax);
        }
        break;
    case EffectType::kAura:
        if (auraObject_) {
            auraObject_->SetPosition(position);
            auraObject_->SetScale(
                {auraConfig_.scale.x * scale.x, auraConfig_.scale.y * scale.y, auraConfig_.scale.z * scale.z});
            auraObject_->SetRotate(rotation);
        }
        break;
    case EffectType::kSwing:
        if (swingObject_) {
            isActive_ = true;
            lifeTimer_ = swingConfig_.lifeTime;
            currentUVOffset_ = {0.0f, 0.0f};
            swingObject_->SetPosition(position);
            swingObject_->SetRotate(rotation);
            swingObject_->SetScale({swingConfig_.startScale.x * scale.x, swingConfig_.startScale.y * scale.y,
                                    swingConfig_.startScale.z * scale.z});
            swingObject_->GetMaterial().color = swingConfig_.color;
        }
        break;
    case EffectType::kExplosion:
        isActive_ = true;
        lifeTimer_ = explosionConfig_.lifeTime;
        currentUVOffset_ = {0.0f, 0.0f};
        if (explosionObject_) {
            explosionObject_->SetPosition(position);
            explosionObject_->SetScale({explosionConfig_.coreStartScale.x * scale.x,
                                        explosionConfig_.coreStartScale.y * scale.y,
                                        explosionConfig_.coreStartScale.z * scale.z});
            explosionObject_->GetMaterial().color = explosionConfig_.color;
        }
        if (explosionWaveObject_) {
            explosionWaveObject_->SetPosition(position);
            explosionWaveObject_->SetScale({explosionConfig_.waveStartScale.x * scale.x,
                                            explosionConfig_.waveStartScale.y * scale.y,
                                            explosionConfig_.waveStartScale.z * scale.z});
            explosionWaveObject_->GetMaterial().color = explosionConfig_.color;
        }
        if (explosionSparkParticle_) {
            int sparkCount = (scale.x < 0.5f) ? 15 : 60;

            Irufemi::Vector3 startScale = {0.05f * scale.x, 0.05f * scale.y, 0.05f * scale.z};
            Irufemi::Vector3 midScale = {0.08f * scale.x, 0.08f * scale.y, 0.08f * scale.z};

            explosionSparkParticle_->SetStartScale(startScale);
            explosionSparkParticle_->SetMidScale(midScale);
            explosionSparkParticle_->SetEndScale({0.0f, 0.0f, 0.0f});

            explosionSparkParticle_->SetPosition(position);
            explosionSparkParticle_->SetRadius(0.1f);
            explosionSparkParticle_->SetVelocity(8.0f);
            explosionSparkParticle_->SetSpread(1.0f);

            explosionSparkParticle_->MarkDirty();
            explosionSparkParticle_->EmitBurst(sparkCount);
        }
        break;
    }
}
