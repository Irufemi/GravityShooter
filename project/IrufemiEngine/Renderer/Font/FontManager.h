#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <d3d12.h>
#include "Core/Math/Vector2.h"
#include "Core/System/TaskGroup.h"
#include "Core/System/ResourceHandle.h"

class IrufemiEngine;

/**
 * @struct GlyphInfo
 * @brief フォントの1文字（グリフ）の描画に必要な情報
 */
struct GlyphInfo {
    char32_t character;
    Irufemi::Vector2 uvTopLeft;
    Irufemi::Vector2 uvBottomRight;
    float width;
    float height;
    float offsetX;  // 描画基準点からのXズレ (Bearing X)
    float offsetY;  // 描画基準点からのYズレ (Bearing Y)
    float advanceX; // 次の文字へのカーソル移動量
};

/**
 * @class FontManager
 * @brief TTFフォントから実行時にMSDFを動的生成し、アトラスとして管理するクラス
 */
class FontManager {
public:
    FontManager();
    ~FontManager();

    // 初期化 (FreeTypeなどの初期化、アトラステクスチャの作成)
    /**
     * @brief Initialize を実行する。
     */
    void Initialize(IrufemiEngine* engine);
    /**
     * @brief Finalize を実行する。
     */
    void Finalize();

    // TTFフォントをロードし、管理対象として登録する
    /**
     * @brief LoadFont を実行する。
     */
    bool LoadFont(const std::string& fontId, const std::string& ttfPath);

    // 指定フォルダ配下を再帰的に走査し、.ttf / .otf を一括ロードする
    /**
     * @brief LoadAllFromFolder を実行する。
     */
    void LoadAllFromFolder(const std::string& folderPath);

    // ロード済みのフォントIDリストを取得する
    /**
     * @brief LoadedFontIds を取得する。
     * @return 取得された LoadedFontIds
     */
    std::vector<std::string> GetLoadedFontIds() const;

    // 文字列を受け取り、まだ生成されていない文字があればMSDFを生成してアトラスに追加する
    /**
     * @brief PrecacheText を実行する (非同期)。
     */
    void PrecacheText(const std::string& fontId, const std::wstring& text);

    /**
     * @brief 同期的にMSDFを生成してアトラスに追加する (UI表示前の文字飛び防止用)
     */
    void PrecacheTextSync(const std::string& fontId, const std::wstring& text);

    // 文字のグリフ情報を取得する (キャッシュにない場合は非同期または即座に生成する)
    /**
     * @brief Glyph を取得する。
     * @return 取得された Glyph (存在しない・生成中の場合は nullopt またはダミー)
     */
    std::optional<GlyphInfo> GetGlyph(const std::string& fontId, char32_t character);

    // シェーダに渡すための動的アトラスのテクスチャハンドルを取得
    /**
     * @brief AtlasHandle を取得する。
     * @return 取得された AtlasHandle
     */
    ResourceHandle GetAtlasHandle() const;

    // 非同期生成タスクがすべて完了しているかを取得
    /**
     * @brief IsAllLoaded かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsAllLoaded() const;

    // アトラスの更新世代（バージョン番号）を取得
    /**
     * @brief AtlasVersion を取得する。
     * @return アトラスがGPU更新されるたびにインクリメントされる世代番号
     */
    uint64_t GetAtlasVersion() const;

private:
    void PrecacheTextInternal(const std::string& fontId, const std::wstring& text);

    // Pimplイディオム：FreeType, msdfgen, stb_rect_pack などの依存を隠蔽する
    struct Impl;
    std::unique_ptr<Impl> impl_;

    IrufemiEngine* engine_ = nullptr;
};
