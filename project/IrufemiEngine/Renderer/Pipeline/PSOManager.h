#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <vector>
#include <shared_mutex>
#include "Core/Type/BlendMode.h"

/**
 * @class PSOManager
 * @brief パイプラインステートオブジェクト（PSO）を管理・キャッシュするクラス
 * @details ブレンドモード、デプス書き込み設定、カリングモードなどの組み合わせに応じて
 *          PSO を生成・キャッシュし、描画時に適切なステートを提供します。
 *          同一の設定セットに対しては生成済みの PSO を再利用することで、
 *          実行時のステート切り替えコストを最適化します。
 */
class PSOManager {
public:
    /** @enum DepthWrite
     *  @brief 深度情報の扱い
     */
    enum class DepthWrite {
        Enable,  ///< 深度書き込み有効
        Disable, ///< 深度テストのみ（書き込み無効）
        Off      ///< 深度テスト・書き込み共に無効
    };

    /** @enum CullMode
     *  @brief カリングモード
     */
    enum class CullMode {
        Back,  ///< 背面カリング
        Front, ///< 前面カリング
        None   ///< カリングなし（両面描画）
    };

    /** @struct ShaderSet
     *  @brief 各シェーダステージのバイナリ（Blob）をまとめた構造体
     */
    struct ShaderSet {
        Microsoft::WRL::ComPtr<IDxcBlob> vsBlob; ///< 頂点シェーダ
        Microsoft::WRL::ComPtr<IDxcBlob> psBlob; ///< ピクセルシェーダ
        Microsoft::WRL::ComPtr<IDxcBlob> gsBlob; ///< ジオメトリシェーダ（任意）
    };

    /** @struct PipelineStateDesc
     *  @brief PSO生成に必要な設定をまとめた構造体
     */
    struct PipelineStateDesc {
        ShaderSet shaders;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        DXGI_FORMAT rtvFormat = DXGI_FORMAT_UNKNOWN;  // UNKNOWNの場合はManagerのデフォルトを使用
        uint32_t numRenderTargets = 1;                ///< MRT対応: レンダリングターゲット数
        DXGI_FORMAT rtvFormat1 = DXGI_FORMAT_UNKNOWN; ///< MRT対応: SV_TARGET1 (Mask)
        DXGI_FORMAT rtvFormat2 = DXGI_FORMAT_UNKNOWN; ///< MRT対応: SV_TARGET2 (Normal)
        DXGI_FORMAT rtvFormat3 = DXGI_FORMAT_UNKNOWN; ///< MRT対応: SV_TARGET3 (Material)
        DXGI_FORMAT rtvFormat4 = DXGI_FORMAT_UNKNOWN; ///< MRT対応: SV_TARGET4 (Velocity)
        DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;  // UNKNOWNの場合、Managerのデフォルトを使用
        bool isDepthOnly = false;                     // シャドウマップなど、RTVを持たないパス用
        bool disableDepthTest = false; // バックバッファ書き込みなど、深度テストを無効化する用
        bool noDSV = false; // バックバッファなど、DSVを一切バインドしないパス用 (DSVFormat = UNKNOWN を強制する)
        bool useNullInputLayout = false; // CopyImageなど、頂点バッファを入力としないパス用
    };

    /**
     * @brief 初期化処理
     * @details デバイスや共通フォーマットを保持します。
     */
    void Initialize(ID3D12Device* device, ID3D12RootSignature* rootSig, const D3D12_INPUT_LAYOUT_DESC& inputLayout,
                    DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat,
                    D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

    /**
     * @brief シェーダの登録
     * @param name シェーダの識別名
     * @param desc PSO生成情報
     */
    void RegisterShader(const std::string& name, const PipelineStateDesc& desc);

    /**
     * @brief 登録済みシェーダを用いたPSOの取得
     * @param name 登録されたシェーダ名
     * @param blend ブレンドモード
     * @param depth 深度の扱い
     * @param cull カリングモード
     * @return キャッシュまたは新規生成されたPSO
     */
    ID3D12PipelineState* GetPSO(const std::string& name, Irufemi::BlendMode blend, DepthWrite depth, CullMode cull);

    /**
     * @brief コンピュートシェーダの登録
     * @param name シェーダの識別名
     * @param csBlob コンピュートシェーダバイナリ
     * @param computeRootSig コンピュート用ルートシグネチャ
     */
    void RegisterComputeShader(const std::string& name, const Microsoft::WRL::ComPtr<IDxcBlob>& csBlob,
                               ID3D12RootSignature* computeRootSig);

    /**
     * @brief 登録済みコンピュートシェーダを用いたPSOの取得
     * @param name 登録されたシェーダ名
     * @return キャッシュされたコンピュートPSO
     */
    ID3D12PipelineState* GetComputePSO(const std::string& name);

    /** @brief 画面コピー用 PSO を取得 */
    ID3D12PipelineState* GetCopyImage();
    ///@}

    /** @brief キャッシュされているすべての PSO を破棄する */
    void ClearCache();

    /** @brief 特定の名前（論理名）に関連付けられた PSO キャッシュのみを破棄する */
    void ClearCacheByName(const std::string& name);

    /** @brief ゲームプレイ中によく使われる PSO の組み合わせを事前にコンパイル・キャッシュします */
    void PreWarmCommonPSOs(class ThreadPool* threadPool = nullptr);

private:
    using ComPtr = Microsoft::WRL::ComPtr<ID3D12PipelineState>;

    // デバイスおよびルートシグネチャ（RS/IL/RTV/DSV/Topology 等は固定）
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig_;
    D3D12_INPUT_LAYOUT_DESC inputLayout_{};

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements_;
    std::vector<std::string> semanticNames_;
    DXGI_FORMAT rtvFormat_{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB};
    DXGI_FORMAT dsvFormat_{DXGI_FORMAT_D24_UNORM_S8_UINT};
    D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_{D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE};

    std::unordered_map<std::string, PipelineStateDesc> shaderRegistry_;
    ShaderSet copyImageShaders_{}; //!< GetCopyImage内部処理用シェーダーセット

    /** @brief キャッシュキー構造体 */
    struct Key {
        uint64_t hash;
        bool operator==(const Key& o) const {
            return hash == o.hash;
        }
    };
    /** @brief キャッシュキーのハッシュ関数 */
    struct KeyHash {
        size_t operator()(const Key& k) const {
            return static_cast<size_t>(k.hash);
        }
    };

    std::unordered_map<Key, ComPtr, KeyHash> cache_; ///< PSO キャッシュ
    std::unordered_map<std::string, std::vector<Key>>
        cacheKeysByName_; ///< ホットリロードのためのキートラッキング (名前ごとの生成済みキャッシュキー)
    std::unordered_map<std::string, ComPtr> computeCache_; ///< Compute PSO キャッシュ
    std::vector<ComPtr> retiredPSOs_; ///< ホットリロード時等に一時退避させて安全に寿命を延ばすための遅延解放リスト
    mutable std::shared_mutex psoMutex_; ///< マルチスレッド並列アクセス保護用ミューテックス

    /** @name 内部生成ヘルパー */
    ///@{
    /** @brief Irufemi::BlendMode から D3D12_BLEND_DESC を作成 */
    static D3D12_BLEND_DESC MakeBlend(Irufemi::BlendMode m);
    /** @brief DepthWrite から D3D12_DEPTH_STENCIL_DESC を作成 */
    static D3D12_DEPTH_STENCIL_DESC MakeDepth(DepthWrite w);

    /** @brief 設定セット（シェーダ、ブレンド、デプス、カリング）からハッシュ値を計算 */
    static uint64_t Hash(const std::string& name, Irufemi::BlendMode b, DepthWrite d, CullMode c);

    /** @name ディスクキャッシュ (PSO Blob) */
    ///@{
    std::vector<uint8_t> LoadCachedBlob(const std::string& cacheFileName) const;
    /**
     * @brief SaveCachedBlob を実行する。
     */
    void SaveCachedBlob(const std::string& cacheFileName, ID3D12PipelineState* pso) const;
    ///@}
    ///@}
};