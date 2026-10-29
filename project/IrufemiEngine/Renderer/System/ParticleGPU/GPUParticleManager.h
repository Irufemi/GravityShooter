#pragma once
#include "Renderer/System/ParticleGPU/GPUParticleSystem.h"
#include "Core/Utility/Synchronized.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include "Core/Type/BlendMode.h"

class GPUParticleManager {
public:
    GPUParticleManager() = default;
    ~GPUParticleManager() = default;

    /**
     * @brief Initialize を実行する。
     */
    void Initialize();
    /**
     * @brief Update を実行する。
     */
    void Update();
    /**
     * @brief Draw を実行する。
     */
    void Draw();
    /**
     * @brief Finalize を実行する。
     */
    void Finalize();
    /**
     * @brief Debug を実行する。
     */
    void Debug();

    /**
     * @brief 現在GPU上に残っているすべてのパーティクルをクリアする（シーン遷移時などに使用）
     */
    void ClearAllParticles();

    /**
     * @brief シーン開始前にパーティクル基盤のウォームアップを行う
     */
    void WarmUp();

    /**
     * @struct EmitterHandle
     * @brief パーティクルエミッターを一意に識別・操作するための不透明ハンドル (Opaque Handle)
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
     * @brief 現在稼働しているパーティクルシステム（テクスチャ）の種類数を取得する。
     * @return アクティブなシステム数
     */
    int GetActiveSystemCount() const {
        return static_cast<int>(state_.Read()->systems.size());
    }

    /**
     * @brief 使用中の全エミッター数を取得する。
     * @return エミッターの総数
     */
    int GetTotalEmittersUsed() const;

    /**
     * @brief 指定したテクスチャ、ブレンドモード、タイムスケール設定に対するシステムを取得し、エミッターを登録する
     */
    EmitterHandle RegisterEmitter(const std::string& texturePath, Irufemi::BlendMode blendMode, bool isUnscaledTime,
                                  bool enableLighting,
                                  PSOManager::DepthWrite depthWrite = PSOManager::DepthWrite::Disable);

    /**
     * @brief 登録したエミッターを解放する
     */
    void UnregisterEmitter(const EmitterHandle& handle);

    /**
     * @brief エミッターデータを更新する
     */
    void UpdateEmitterData(const EmitterHandle& handle, const GPUParticleEmitter& data);
    /**
     * @brief MeshEmitterBuffer を設定する。
     * @param[in] handle 設定する MeshEmitterBuffer の値
     * @param[in] vbAddress 設定する MeshEmitterBuffer の値
     */
    void SetMeshEmitterBuffer(EmitterHandle handle, D3D12_GPU_VIRTUAL_ADDRESS vbAddress);

    /** @name Field Management */
    ///@{
    struct FieldHandle {
        static constexpr uint32_t kInvalidIndex = 0xFFFFFFFF;
        uint32_t index = kInvalidIndex;
        /**
         * @brief IsValid かどうかを判定する。
         * @return 判定結果 (true/false)
         */
        bool IsValid() const {
            return index != kInvalidIndex;
        }
    };
    /**
     * @brief RegisterField を実行する。
     */
    FieldHandle RegisterField();
    /**
     * @brief UnregisterField を実行する。
     */
    void UnregisterField(const FieldHandle& handle);
    /**
     * @brief UpdateFieldData を実行する。
     */
    void UpdateFieldData(const FieldHandle& handle, const ParticleField& data);
    ///@}

private:
    GPUParticleManager(const GPUParticleManager&) = delete;
    GPUParticleManager& operator=(const GPUParticleManager&) = delete;

    struct SystemContext {
        uint32_t systemId = 0;
        std::unique_ptr<GPUParticleSystem> system;
        std::vector<uint32_t> freeIndices;
        std::vector<uint16_t> slotGenerations;
        uint32_t nextIndex = 0;
    };

    struct SystemKey {
        std::string texturePath;
        Irufemi::BlendMode blendMode;
        bool isUnscaledTime;
        bool enableLighting;
        PSOManager::DepthWrite depthWrite;

        bool operator==(const SystemKey& other) const {
            return texturePath == other.texturePath && blendMode == other.blendMode &&
                   isUnscaledTime == other.isUnscaledTime && enableLighting == other.enableLighting &&
                   depthWrite == other.depthWrite;
        }
    };

    struct SystemKeyHasher {
        std::size_t operator()(const SystemKey& k) const {
            std::size_t h1 = std::hash<std::string>()(k.texturePath);
            std::size_t h2 = std::hash<int>()(static_cast<int>(k.blendMode));
            std::size_t h3 = std::hash<bool>()(k.isUnscaledTime);
            std::size_t h4 = std::hash<bool>()(k.enableLighting);
            std::size_t h5 = std::hash<int>()(static_cast<int>(k.depthWrite));
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
        }
    };

    struct ManagerState {
        std::unordered_map<SystemKey, SystemContext, SystemKeyHasher> systems;
        /**
         * @brief システムIDからシステムコンテキストへの高速逆引きマップ (O(1) 解除・更新用)
         */
        std::unordered_map<uint32_t, SystemContext*> idLookup;
        uint32_t nextSystemId = 1;

        // グローバルなField管理
        std::vector<ParticleField> globalFields;
        std::vector<uint32_t> freeFieldIndices;
        uint32_t nextFieldIndex = 0;
    };

    mutable Irufemi::Synchronized<ManagerState> state_;
};
