#pragma once

#include "Renderer/System/VoxelParticle/VoxelParticleSystem.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "Core/Math/Vector3Int.h"

class IrufemiEngine;
namespace Irufemi {
struct OBB;
}

class VoxelParticleManager {
public:
    VoxelParticleManager() = default;
    ~VoxelParticleManager() = default;

    /**
     * @brief Initialize を実行する。
     */
    void Initialize(IrufemiEngine* engine);
    /**
     * @brief Update を実行する。
     */
    void Update(float deltaTime);
    /**
     * @brief Draw を実行する。
     */
    void Draw();
    /**
     * @brief Clear を実行する。
     */
    void Clear();

    /**
     * @brief シーン開始前に基本モデルやComputeパイプラインを事前ウォームアップする
     */
    void WarmUp();

    /**
     * @struct EmitterHandle
     * @brief ボクセルパーティクルエミッターを一意に識別・操作するための不透明ハンドル (Opaque Handle)
     */
    struct EmitterHandle {
        uint32_t systemId = 0;          ///< システム一意ID（0は無効値）
        uint16_t emitterIndex = 0xFFFF; ///< スロット番号
        uint16_t generation = 0;        ///< スロット世代番号（解放・再利用の検知用）

        /**
         * @brief 有効なハンドルかどうかを判定する。
         * @return 判定結果 (true/false)
         */
        bool IsValid() const {
            return systemId != 0 && emitterIndex != 0xFFFF;
        }

        bool operator==(const EmitterHandle& other) const {
            return systemId == other.systemId && emitterIndex == other.emitterIndex && generation == other.generation;
        }
        bool operator!=(const EmitterHandle& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief 指定したモデルと解像度に対するシステムを取得し、エミッター（インスタンス）を登録する
     */
    EmitterHandle RegisterEmitter(const std::string& modelName, const Irufemi::Vector3Int& resolution);

    /**
     * @brief 登録したエミッターを解放する
     */
    void UnregisterEmitter(const EmitterHandle& handle);

    /**
     * @brief エミッターデータを更新する
     */
    void UpdateEmitterData(const EmitterHandle& handle, const VoxelEmitter& data);
    /**
     * @brief EmitterData を取得する。
     * @return 取得された EmitterData
     */
    const VoxelEmitter& GetEmitterData(const EmitterHandle& handle) const;

    /**
     * @brief 事前に指定された数のパーティクルシステムをロード・確保する
     */
    void ReservePool(const std::string& modelName, const Irufemi::Vector3Int& resolution, int preAllocateCount = 1000);

    /**
     * @brief 全ての登録済みボクセルシステムがロード・初期化完了しているかを判定する
     */
    bool IsAllReady() const;

    /**
     * @brief 登録済みの全ボクセルシステムの初期化（ボクセル化＋GPUリソース確保）をロード画面中に同期完了させる
     */
    void EnsureAllInitialized();

    /**
     * @brief その場での爆発エフェクトを発生させる
     */
    void PlayExplosion(const std::string& modelName, const Irufemi::Vector3& worldPos, const Irufemi::Vector3& velocity,
                       const Irufemi::Vector3& rotate, const Irufemi::Vector3& scale, const VoxelEmitter& params,
                       const Irufemi::Vector3Int& resolution);

    uint32_t GetActiveSystemCount() const {
        return static_cast<uint32_t>(systems_.size());
    }
    uint32_t GetTotalEmittersUsed() const {
        uint32_t count = 0;
        for (const auto& pair : systems_) {
            count += pair.second.nextIndex - static_cast<uint32_t>(pair.second.freeIndices.size());
        }
        return count;
    }

private:
    VoxelParticleManager(const VoxelParticleManager&) = delete;
    VoxelParticleManager& operator=(const VoxelParticleManager&) = delete;
    VoxelParticleManager(VoxelParticleManager&&) = delete;
    VoxelParticleManager& operator=(VoxelParticleManager&&) = delete;

    struct SystemKey {
        std::string modelName;
        Irufemi::Vector3Int resolution;

        bool operator==(const SystemKey& other) const {
            return modelName == other.modelName && resolution.x == other.resolution.x &&
                   resolution.y == other.resolution.y && resolution.z == other.resolution.z;
        }
    };

    struct OneShotEmitter {
        EmitterHandle handle;
        float emitTimer;
        float lifeTimer;
    };

    struct SystemKeyHasher {
        std::size_t operator()(const SystemKey& k) const {
            std::size_t seed = std::hash<std::string>()(k.modelName);
            auto hashCombine = [](std::size_t& s, int v) {
                s ^= std::hash<int>()(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
            };
            hashCombine(seed, k.resolution.x);
            hashCombine(seed, k.resolution.y);
            hashCombine(seed, k.resolution.z);
            return seed;
        }
    };

    struct SystemContext {
        uint32_t systemId = 0;
        std::unique_ptr<VoxelParticleSystem> system;
        std::vector<uint32_t> freeIndices;
        std::vector<uint16_t> slotGenerations;
        uint32_t nextIndex = 0;
    };

    std::unordered_map<SystemKey, SystemContext, SystemKeyHasher> systems_;
    /**
     * @brief システムIDからシステムコンテキストへの高速逆引きマップ (O(1) 解除・更新用)
     */
    std::unordered_map<uint32_t, SystemContext*> idLookup_;
    uint32_t nextSystemId_ = 1;
    std::vector<OneShotEmitter> oneShots_;
    IrufemiEngine* engine_ = nullptr;
};
