#pragma once

class GravityPlayerComponent;

/**
 * @class IPlayerCommand
 * @brief プレイヤーのアクション（引き寄せ、投擲等）をカプセル化する Command パターン基底インタフェース
 */
class IPlayerCommand {
public:
    virtual ~IPlayerCommand() = default;

    /**
     * @brief コマンドの実行
     * @param[in] player 操作対象のプレイヤーコンポーネント
     */
    virtual void Execute(GravityPlayerComponent* player) = 0;
};
