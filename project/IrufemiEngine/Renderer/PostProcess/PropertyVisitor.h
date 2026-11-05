#pragma once

#include "Core/Math/MathFunction.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include <cstdint>
#include "imgui/imgui.h"

namespace Irufemi {

/**
 * @class ImGuiPropertyVisitor
 * @brief パラメータ構造体のプロパティをImGuiウィジェットとして描画するVisitor
 * @details 各パラメータ構造体の Visit メソッドから呼び出され、UIコントロールを展開します。
 */
class ImGuiPropertyVisitor {
public:
    ImGuiPropertyVisitor() = default;
    ~ImGuiPropertyVisitor() = default;

    /**
     * @brief 浮動小数点数ドラッグ入力
     */
    void DragFloat(const char* label, float& value, float speed = 0.01f, float min = 0.0f, float max = 1.0f) {
        ImGui::DragFloat(label, &value, speed, min, max);
    }

    /**
     * @brief 2次元ベクトルドラッグ入力
     */
    void DragFloat2(const char* label, Irufemi::Vector2& value, float speed = 0.01f, float min = 0.0f, float max = 1.0f) {
        ImGui::DragFloat2(label, &value.x, speed, min, max);
    }

    /**
     * @brief 3次元ベクトルドラッグ入力
     */
    void DragFloat3(const char* label, Irufemi::Vector3& value, float speed = 0.01f, float min = 0.0f, float max = 1.0f) {
        ImGui::DragFloat3(label, &value.x, speed, min, max);
    }

    /**
     * @brief 4次元ベクトルドラッグ入力
     */
    void DragFloat4(const char* label, Irufemi::Vector4& value, float speed = 0.01f, float min = 0.0f, float max = 1.0f) {
        ImGui::DragFloat4(label, &value.x, speed, min, max);
    }

    /**
     * @brief 4要素カラーピッカー入力 (RGBA)
     */
    void ColorEdit4(const char* label, Irufemi::Vector4& value) {
        ImGui::ColorEdit4(label, &value.x);
    }

    /**
     * @brief 浮動小数点数スライダ入力
     */
    void SliderFloat(const char* label, float& value, float min, float max) {
        ImGui::SliderFloat(label, &value, min, max);
    }

    /**
     * @brief 整数スライダ入力
     */
    void SliderInt(const char* label, int& value, int min, int max) {
        ImGui::SliderInt(label, &value, min, max);
    }

    /**
     * @brief 奇数カーネルサイズ専用スライダ入力 (偶数を自動回避)
     */
    void SliderKernelSize(const char* label, int32_t& value, int min, int max) {
        if (ImGui::SliderInt(label, reinterpret_cast<int*>(&value), min, max)) {
            if (value < min) {
                value = min;
            }
            if (value > 1 && value % 2 == 0) {
                value += 1;
            }
        }
    }

    /**
     * @brief 角度スライダ入力 (ラジアンと度数法の自動相互変換)
     */
    void SliderAngle(const char* label, float& radians, float minDeg = -180.0f, float maxDeg = 180.0f) {
        float angleDeg = Irufemi::Math::ToDegree(radians);
        if (ImGui::SliderFloat(label, &angleDeg, minDeg, maxDeg)) {
            radians = Irufemi::Math::ToRadian(angleDeg);
        }
    }

    /**
     * @brief ドロップダウンコンボボックス選択
     */
    void Combo(const char* label, int32_t& value, const char* const items[], int itemCount) {
        ImGui::Combo(label, reinterpret_cast<int*>(&value), items, itemCount);
    }
};

} // namespace Irufemi

