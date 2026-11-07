#pragma once

#include "Platform/Input/InputAction.h"
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @class InputMappingContext
 * @brief アクションと物理入力（バインディング）の対応付けを管理するクラス
 * @details 状態（Gameplay, UI, Pause等）ごとの入力プロファイルを表現し、InputManagerにより優先度スタック管理されます。
 */
class InputMappingContext {
public:
    /**
     * @brief コンストラクタ
     * @param[in] name コンテキスト名（例: "Gameplay", "UI", "PauseMenu"）
     */
    explicit InputMappingContext(std::string name = "Default") : name_(std::move(name)) {}
    ~InputMappingContext() = default;

    /** @brief コンテキスト名を取得する */
    const std::string& GetName() const {
        return name_;
    }

    /** @brief コンテキスト名を設定する */
    void SetName(const std::string& name) {
        name_ = name;
    }

    /**
     * @brief アクションに対して新しいバインディングを追加する
     * @param[in] actionName アクション名（例: "Jump", "Move"）
     * @param[in] binding 割り当てる物理入力の設定
     */
    void AddBinding(const std::string& actionName, const InputBinding& binding);

    /**
     * @brief 指定したアクション名に紐づくすべてのバインディングを取得する
     * @param[in] actionName アクション名
     * @return バインディングのリスト（存在しない場合は空のリストを返す）
     */
    const std::vector<InputBinding>& GetBindings(const std::string& actionName) const;

    /**
     * @brief 登録されているすべてのマッピング（アクション名とバインディング一覧のペア）を取得する
     * @return マッピング連想配列への const 参照
     */
    const std::unordered_map<std::string, std::vector<InputBinding>>& GetMappings() const {
        return mappings_;
    }

    /**
     * @brief 登録されているすべてのアクション名を取得する
     * @return アクション名のリスト
     */
    std::vector<std::string> GetAllActionNames() const;

    /**
     * @brief すべてのバインディングをクリアする
     */
    void Clear();

private:
    std::string name_{"Default"};
    std::unordered_map<std::string, std::vector<InputBinding>> mappings_{};
};
