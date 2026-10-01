#include "Framework/Utility/CVar.h"

// ============================================================================
// EngineCVars.cpp
//
// エンジン側の設定項目（Console Variables）を一元管理するファイルです。
// 初期化順序を保証し、どこにどんな設定があるかを一覧化するため、
// ここでマクロを用いて一括定義します。
// ============================================================================

// --- Display Settings ---
DEFINE_CVAR_INT("r.DisplayMode", 0, "Display Mode (0=Windowed, 1=Borderless)");
DEFINE_CVAR_BOOL("r.VSync", true, "Enable Vertical Synchronization");

// --- Audio Settings (将来用) ---
DEFINE_CVAR_FLOAT("a.MasterVolume", 1.0f, "Master Volume multiplier");
DEFINE_CVAR_FLOAT("a.BGMVolume", 1.0f, "BGM Volume multiplier");
DEFINE_CVAR_FLOAT("a.SEVolume", 1.0f, "SE Volume multiplier");

// --- Developer / Debug Settings ---
DEFINE_CVAR_BOOL("d.ShowFPS", false, "Show FPS counter on screen");

// 静的ライブラリでリンク落ちを防ぐためのダミー関数
namespace Irufemi {
void ReferenceEngineCVars() {}
} // namespace Irufemi
