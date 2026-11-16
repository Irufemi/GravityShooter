#pragma once

#include "Framework/Scene/BaseScene.h"
#include <string>
#include <memory>
#include <windows.h>
#include <dxcapi.h>
#include <wrl.h>
#include "Scenes/TL1/MagicBrushClient.h"

class IrufemiEngine;

/**
 * @brief ツール開発・検証用の専用シーン
 */
class TL1Scene : public Irufemi::BaseScene {
public: // メンバ関数
    TL1Scene() = default;
    ~TL1Scene() override;

    void Initialize(IrufemiEngine* engine) override;
    void Update() override;
    void Draw() override;
    void DrawStandaloneDebugWindows() override;

private: // メンバ変数
    std::string promptText_ = "";
    std::string referenceImagePath_ = ""; // AI参考画像用
    std::string textureImagePath_ = "";   // C++入力テクスチャ用

    std::string shaderName_ = "MagicBrushPS";                      // 保存・登録用のシェーダー名
    std::string outputDirectory_ = "resources/shaders/generated/"; // 出力先フォルダ

    std::unique_ptr<MagicBrushClient> magicBrushClient_;

    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob_ = nullptr;
    bool isShaderRegistered_ = false;
};
