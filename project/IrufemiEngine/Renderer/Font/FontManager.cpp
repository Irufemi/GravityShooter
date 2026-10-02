#include "Renderer/Font/FontManager.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Core/System/ThreadPool.h"
#include "Core/Utility/Log.h"
#include <iostream>

// --- 外部ライブラリ群 ---
#include <ft2build.h>
#include FT_FREETYPE_H

#undef min
#undef max
#include <msdfgen/msdfgen.h>
#include <msdfgen/ext/import-font.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>

#include <DirectXTex/DirectXTex.h>
#include "RHI/DirectX12/DirectXUtils.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include "Resource/Texture/TextureManager.h"

#include <unordered_map>
#include <vector>
#include <mutex>
#include <filesystem>
#include <algorithm>

// 内部実装の定義
struct FontManager::Impl {
    msdfgen::FreetypeHandle* ftLibrary = nullptr;
    std::unordered_map<std::string, msdfgen::FontHandle*> fonts;

    // キャッシュされた文字情報: FontID -> (文字コード -> GlyphInfo)
    std::unordered_map<std::string, std::unordered_map<char32_t, GlyphInfo>> glyphCache;
    std::mutex cacheMutex;

    // アトラス管理 (stb_rect_pack用)
    stbrp_context packContext;
    std::vector<stbrp_node> packNodes;
    std::vector<uint8_t> cpuAtlasData; // RGBA

    // DirectX12 用の動的テクスチャリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> atlasTexture;
    ResourceHandle atlasHandle{};

    static const int ATLAS_WIDTH = 2048;
    static const int ATLAS_HEIGHT = 2048;
    static const int GLYPH_SIZE = 32;       // ベースサイズ
    static const int PADDING = 2;           // エッジパディング
    static constexpr double PX_RANGE = 2.0; // 距離場スプレッド

    // 非同期生成用スレッドプールとタスクグループ
    std::unique_ptr<ThreadPool> threadPool;
    std::shared_ptr<TaskGroup> taskGroup;

    // アトラスの更新世代番号
    std::atomic<uint64_t> atlasVersion{0};
};

FontManager::FontManager() : impl_(std::make_unique<Impl>()) {}

FontManager::~FontManager() {
    Finalize();
}

void FontManager::Initialize(IrufemiEngine* engine) {
    engine_ = engine;

    // FreeTypeの初期化
    impl_->ftLibrary = msdfgen::initializeFreetype();

    // CPUアトラスの初期化
    impl_->cpuAtlasData.resize(Impl::ATLAS_WIDTH * Impl::ATLAS_HEIGHT * 4, 0); // 0クリア(RGBA)

    // stb_rect_pack のコンテキスト初期化
    impl_->packNodes.resize(Impl::ATLAS_WIDTH);
    stbrp_init_target(&impl_->packContext, Impl::ATLAS_WIDTH, Impl::ATLAS_HEIGHT, impl_->packNodes.data(),
                      static_cast<int>(impl_->packNodes.size()));

    // 2048x2048 のアトラステクスチャ生成 (DirectX12)
    DirectX::TexMetadata metadata{};
    metadata.width = Impl::ATLAS_WIDTH;
    metadata.height = Impl::ATLAS_HEIGHT;
    metadata.depth = 1;
    metadata.arraySize = 1;
    metadata.mipLevels = 1;
    metadata.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    metadata.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;

    impl_->atlasTexture = engine_->GetDirectXCommon()->CreateTextureResource(metadata);

    // デスクリプタの確保とSRVの作成
    DescriptorPool* pool = engine_->GetDirectXCommon()->GetSrvPool();
    uint32_t srvIndex = pool->Allocate();
    pool->CreateSRVForTexture2D(srvIndex, impl_->atlasTexture.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 1);
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = pool->GetGPUHandle(srvIndex);

    // TextureManagerに登録してResourceHandleを受け取る
    impl_->atlasHandle =
        engine_->GetTextureManager()->RegisterExternalTexture("FontAtlas", impl_->atlasTexture, srvIndex, srvHandle);

    // テクスチャ作成直後は COPY_DEST なので、後続の更新処理（GENERIC_READ -> COPY_DEST）に
    // 合わせるために一度 GENERIC_READ に遷移しておく
    engine_->GetDirectXCommon()->ExecuteUploadCommands([&](ID3D12GraphicsCommandList* cmdList) {
        DirectXUtils::TransitionBarrier(cmdList, impl_->atlasTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                        D3D12_RESOURCE_STATE_GENERIC_READ);
    });

    // スレッドプールの初期化 (1スレッド) とタスクグループの初期化
    impl_->threadPool = std::make_unique<ThreadPool>(1);
    impl_->taskGroup = std::make_shared<TaskGroup>();
}

void FontManager::Finalize() {
    impl_->threadPool.reset(); // スレッドプールの停止を待機

    for (auto& pair : impl_->fonts) {
        if (pair.second) {
            msdfgen::destroyFont(pair.second);
        }
    }
    impl_->fonts.clear();

    if (impl_->ftLibrary) {
        msdfgen::deinitializeFreetype(impl_->ftLibrary);
        impl_->ftLibrary = nullptr;
    }
}

bool FontManager::LoadFont(const std::string& fontId, const std::string& ttfPath) {
    msdfgen::FontHandle* font = msdfgen::loadFont(impl_->ftLibrary, ttfPath.c_str());
    if (font) {
        std::lock_guard<std::mutex> lock(impl_->cacheMutex);
        impl_->fonts[fontId] = font;
        return true;
    }
    return false;
}

void FontManager::LoadAllFromFolder(const std::string& folderPath) {
    if (!std::filesystem::exists(folderPath)) {
        return;
    }

    for (auto& entry : std::filesystem::recursive_directory_iterator(folderPath)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        auto p = entry.path();
        auto ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".ttf" || ext == ".otf") {
            // ファイル名(拡張子なし)を fontId としてロードする
            std::string fontId = p.stem().string();
            std::string ttfPath = p.generic_string();
            LoadFont(fontId, ttfPath);
        }
    }
}

std::vector<std::string> FontManager::GetLoadedFontIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : impl_->fonts) {
        ids.push_back(pair.first);
    }
    return ids;
}

bool FontManager::IsAllLoaded() const {
    return impl_->taskGroup->IsAllDone();
}

void FontManager::PrecacheText(const std::string& fontId, const std::wstring& text) {
    // 非同期でタスクを投げる（シーンのロード画面等で待機可能にするため）
    impl_->threadPool->Enqueue(impl_->taskGroup, [this, fontId, text]() { PrecacheTextInternal(fontId, text); });
}

void FontManager::PrecacheTextSync(const std::string& fontId, const std::wstring& text) {
    PrecacheTextInternal(fontId, text);
}

void FontManager::PrecacheTextInternal(const std::string& fontId, const std::wstring& text) {
    std::lock_guard<std::mutex> lock(impl_->cacheMutex);
    auto it = impl_->fonts.find(fontId);
    if (it == impl_->fonts.end()) {
        return;
    }
    msdfgen::FontHandle* font = it->second;

    auto& fontCache = impl_->glyphCache[fontId];

    msdfgen::FontMetrics metrics;
    msdfgen::getFontMetrics(metrics, font, msdfgen::FONT_SCALING_NONE);

    // バッチ転送用のダーティ矩形
    bool hasNewGlyphs = false;
    int dirtyMinX = Impl::ATLAS_WIDTH;
    int dirtyMinY = Impl::ATLAS_HEIGHT;
    int dirtyMaxX = 0;
    int dirtyMaxY = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t c1 = text[i];
        char32_t char32 = 0;
        if (c1 >= 0xD800 && c1 <= 0xDBFF && (i + 1 < text.size())) {
            wchar_t c2 = text[i + 1];
            if (c2 >= 0xDC00 && c2 <= 0xDFFF) {
                char32 = 0x10000 + ((static_cast<char32_t>(c1) - 0xD800) << 10) + (static_cast<char32_t>(c2) - 0xDC00);
                ++i;
            } else {
                char32 = static_cast<char32_t>(c1);
            }
        } else {
            char32 = static_cast<char32_t>(c1);
        }

        auto cacheIt = fontCache.find(char32);
        // キャッシュに存在しない、またはダミー（width < 0）の場合は生成
        if (cacheIt == fontCache.end() || cacheIt->second.width < 0.0f) {
            msdfgen::Shape shape;
            double advance = 0.0;

            // スペースなどの空文字対策
            if (char32 == U' ') {
                double spaceAdvance = 0.0, tabAdvance = 0.0;
                msdfgen::getFontWhitespaceWidth(spaceAdvance, tabAdvance, font, msdfgen::FONT_SCALING_NONE);
                double scale = static_cast<double>(Impl::GLYPH_SIZE) / (metrics.emSize > 0.0 ? metrics.emSize : 1.0);
                GlyphInfo info{};
                info.character = char32;
                info.advanceX = static_cast<float>(spaceAdvance * scale);
                fontCache[char32] = info;
                continue;
            }

            if (msdfgen::loadGlyph(shape, font, char32, msdfgen::FONT_SCALING_NONE, &advance)) {
                shape.normalize();
                shape.orientContours();
                msdfgen::edgeColoringSimple(shape, 3.0);

                msdfgen::Shape::Bounds bounds = shape.getBounds();
                // フォントの高さと幅を算出
                double width = bounds.r - bounds.l;
                double height = bounds.t - bounds.b;

                // スケールの決定 (GLYPH_SIZE に収まるように)
                double scale = static_cast<double>(Impl::GLYPH_SIZE) / (metrics.emSize > 0.0 ? metrics.emSize : 1.0);

                int texWidth = static_cast<int>(width * scale) + Impl::PADDING * 2;
                int texHeight = static_cast<int>(height * scale) + Impl::PADDING * 2;

                if (texWidth <= 0 || texHeight <= 0) {
                    continue;
                }

                // オフセット計算 (テクスチャ中央に配置)
                msdfgen::Vector2 translate(-bounds.l + (Impl::PADDING / scale), -bounds.b + (Impl::PADDING / scale));

                // MSDF用ビットマップ
                msdfgen::Bitmap<float, 3> msdf(texWidth, texHeight);
                msdfgen::generateMSDF(msdf, shape, msdfgen::Projection(msdfgen::Vector2(scale), translate),
                                      Impl::PX_RANGE, msdfgen::MSDFGeneratorConfig());

                // stbrp_pack_rects でパッキング
                stbrp_rect rect{};
                rect.w = texWidth;
                rect.h = texHeight;
                if (stbrp_pack_rects(&impl_->packContext, &rect, 1) == 1) {
                    // CPUアトラスにコピー (上下反転に対応するためY座標を反転しながらコピー)
                    for (int y = 0; y < texHeight; ++y) {
                        for (int x = 0; x < texWidth; ++x) {
                            const float* pixel = msdf(x, texHeight - 1 - y); // Y反転
                            int destX = rect.x + x;
                            int destY = rect.y + y;
                            int destIndex = (destY * Impl::ATLAS_WIDTH + destX) * 4;
                            float pxRange = static_cast<float>(Impl::PX_RANGE);
                            impl_->cpuAtlasData[destIndex + 0] =
                                static_cast<uint8_t>(std::clamp((pixel[0] / pxRange + 0.5f) * 255.f, 0.f, 255.f)); // R
                            impl_->cpuAtlasData[destIndex + 1] =
                                static_cast<uint8_t>(std::clamp((pixel[1] / pxRange + 0.5f) * 255.f, 0.f, 255.f)); // G
                            impl_->cpuAtlasData[destIndex + 2] =
                                static_cast<uint8_t>(std::clamp((pixel[2] / pxRange + 0.5f) * 255.f, 0.f, 255.f)); // B
                            impl_->cpuAtlasData[destIndex + 3] = 255;                                              // A
                        }
                    }

                    // GlyphInfo作成
                    GlyphInfo info{};
                    info.character = char32;
                    info.uvTopLeft = Irufemi::Vector2(static_cast<float>(rect.x) / Impl::ATLAS_WIDTH,
                                                      static_cast<float>(rect.y) / Impl::ATLAS_HEIGHT);
                    info.uvBottomRight = Irufemi::Vector2(static_cast<float>(rect.x + rect.w) / Impl::ATLAS_WIDTH,
                                                          static_cast<float>(rect.y + rect.h) / Impl::ATLAS_HEIGHT);
                    info.width = static_cast<float>(rect.w);
                    info.height = static_cast<float>(rect.h);
                    info.offsetX = static_cast<float>(bounds.l * scale) - Impl::PADDING;
                    info.offsetY = static_cast<float>((metrics.ascenderY - bounds.t) * scale) -
                                   Impl::PADDING; // ベースライン基準のオフセット
                    info.advanceX = static_cast<float>(advance * scale);

                    fontCache[char32] = info;

                    // ダーティ領域の更新
                    hasNewGlyphs = true;
                    dirtyMinX = (std::min)(dirtyMinX, static_cast<int>(rect.x));
                    dirtyMinY = (std::min)(dirtyMinY, static_cast<int>(rect.y));
                    dirtyMaxX = (std::max)(dirtyMaxX, static_cast<int>(rect.x + rect.w));
                    dirtyMaxY = (std::max)(dirtyMaxY, static_cast<int>(rect.y + rect.h));
                } else {
                    // アトラスがいっぱいの場合は、毎フレーム再生成を試みるのを防ぐため、仮の文字として登録
                    /**
                     * @brief エディタのコンソールパネルにも出力するため、Log::OutPutLog を使用
                     */
                    Log::OutPutLog(std::cerr, "Font atlas is full! Could not pack glyph for character.");
                    GlyphInfo info{};
                    info.character = char32;
                    info.width = 0.0f;                                   // 描画されない
                    info.advanceX = static_cast<float>(advance * scale); // 幅だけは進める
                    fontCache[char32] = info;
                }
            }
        }
    }

    // --- VRAM(GPU)への一括バッチ転送 ---
    if (hasNewGlyphs && engine_ && engine_->GetDirectXCommon()) {
        int dirtyW = dirtyMaxX - dirtyMinX;
        int dirtyH = dirtyMaxY - dirtyMinY;
        if (dirtyW > 0 && dirtyH > 0) {
            uint32_t alignedRowPitch = (dirtyW * 4 + 255) & ~255;
            uint32_t slicePitch = alignedRowPitch * dirtyH;

            auto intermediateResource = engine_->GetDirectXCommon()->CreateBufferResource(slicePitch);
            if (intermediateResource) {
                uint8_t* mappedData = nullptr;
                if (SUCCEEDED(intermediateResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)))) {
                    for (int y = 0; y < dirtyH; ++y) {
                        int srcY = dirtyMinY + y;
                        int srcIndex = (srcY * Impl::ATLAS_WIDTH + dirtyMinX) * 4;
                        std::memcpy(mappedData + y * alignedRowPitch, &impl_->cpuAtlasData[srcIndex], dirtyW * 4);
                    }
                    intermediateResource->Unmap(0, nullptr);

                    engine_->GetDirectXCommon()->ExecuteUploadCommands([&](ID3D12GraphicsCommandList* cmdList) {
                        DirectXUtils::TransitionBarrier(cmdList, impl_->atlasTexture.Get(),
                                                        D3D12_RESOURCE_STATE_GENERIC_READ,
                                                        D3D12_RESOURCE_STATE_COPY_DEST);

                        D3D12_TEXTURE_COPY_LOCATION dst{};
                        dst.pResource = impl_->atlasTexture.Get();
                        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                        dst.SubresourceIndex = 0;

                        D3D12_TEXTURE_COPY_LOCATION src{};
                        src.pResource = intermediateResource.Get();
                        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                        src.PlacedFootprint.Offset = 0;
                        src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                        src.PlacedFootprint.Footprint.Width = dirtyW;
                        src.PlacedFootprint.Footprint.Height = dirtyH;
                        src.PlacedFootprint.Footprint.Depth = 1;
                        src.PlacedFootprint.Footprint.RowPitch = alignedRowPitch;

                        D3D12_BOX srcBox{};
                        srcBox.left = 0;
                        srcBox.top = 0;
                        srcBox.right = dirtyW;
                        srcBox.bottom = dirtyH;
                        srcBox.front = 0;
                        srcBox.back = 1;

                        cmdList->CopyTextureRegion(&dst, dirtyMinX, dirtyMinY, 0, &src, &srcBox);

                        DirectXUtils::TransitionBarrier(cmdList, impl_->atlasTexture.Get(),
                                                        D3D12_RESOURCE_STATE_COPY_DEST,
                                                        D3D12_RESOURCE_STATE_GENERIC_READ);
                    });

                    engine_->GetDirectXCommon()->ReleaseAfterFence(intermediateResource);

                    // GPUアトラスが更新されたため世代番号をインクリメント
                    ++impl_->atlasVersion;
                }
            }
        }
    }
}

std::optional<GlyphInfo> FontManager::GetGlyph(const std::string& fontId, char32_t character) {
    std::lock_guard<std::mutex> lock(impl_->cacheMutex);
    auto& fontCache = impl_->glyphCache[fontId];
    auto it = fontCache.find(character);

    // すでに正常な文字データが存在する場合はそれを返す
    if (it != fontCache.end()) {
        return it->second;
    }

    // ダミー登録 (複数スレッドからの二重生成リクエスト防止)
    // SDF描画は非同期にしつつ、メトリクス(advanceX)は即座に取得してレイアウト破綻を防ぐ
    GlyphInfo dummy{};
    dummy.character = character;
    dummy.width = -1.0f; // 未生成状態を示すフラグとして width = -1 を使用

    auto fontIt = impl_->fonts.find(fontId);
    if (fontIt != impl_->fonts.end()) {
        msdfgen::FontHandle* font = fontIt->second;
        msdfgen::FontMetrics metrics;
        msdfgen::getFontMetrics(metrics, font, msdfgen::FONT_SCALING_NONE);
        double scale = static_cast<double>(Impl::GLYPH_SIZE) / (metrics.emSize > 0.0 ? metrics.emSize : 1.0);
        if (character == U' ') {
            double spaceAdvance = 0.0, tabAdvance = 0.0;
            msdfgen::getFontWhitespaceWidth(spaceAdvance, tabAdvance, font, msdfgen::FONT_SCALING_NONE);
            dummy.advanceX = static_cast<float>(spaceAdvance * scale);
        } else {
            double advance = 0.0;
            msdfgen::Shape shape;
            if (msdfgen::loadGlyph(shape, font, character, msdfgen::FONT_SCALING_NONE, &advance)) {
                dummy.advanceX = static_cast<float>(advance * scale);
            }
        }
    }
    fontCache[character] = dummy;

    // 非同期で直接内部処理をタスクキューイング
    std::wstring singleChar;
    if (character <= 0xFFFF) {
        singleChar += static_cast<wchar_t>(character);
    } else {
        char32_t code = character - 0x10000;
        singleChar += static_cast<wchar_t>((code >> 10) + 0xD800);
        singleChar += static_cast<wchar_t>((code & 0x3FF) + 0xDC00);
    }

    impl_->threadPool->Enqueue(impl_->taskGroup,
                               [this, fontId, singleChar]() { PrecacheTextInternal(fontId, singleChar); });

    return dummy;
}

ResourceHandle FontManager::GetAtlasHandle() const {
    return impl_->atlasHandle;
}

uint64_t FontManager::GetAtlasVersion() const {
    return impl_->atlasVersion.load();
}
