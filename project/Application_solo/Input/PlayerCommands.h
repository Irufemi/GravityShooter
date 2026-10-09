#pragma once
#include "Input/IPlayerCommand.h"

/**
 * @class PullDebrisCommand
 * @brief ガレキ引き寄せアクション（Command Pattern）
 */
class PullDebrisCommand : public IPlayerCommand {
public:
    PullDebrisCommand() = default;
    ~PullDebrisCommand() override = default;

    void Execute(GravityPlayerComponent* player) override;
};

/**
 * @class ThrowDebrisCommand
 * @brief ガレキ投擲・射出アクション（Command Pattern）
 */
class ThrowDebrisCommand : public IPlayerCommand {
public:
    ThrowDebrisCommand() = default;
    ~ThrowDebrisCommand() override = default;

    void Execute(GravityPlayerComponent* player) override;
};

/**
 * @class MarkTargetCommand
 * @brief ターゲットマーキングアクション（Command Pattern）
 */
class MarkTargetCommand : public IPlayerCommand {
public:
    MarkTargetCommand() = default;
    ~MarkTargetCommand() override = default;

    void Execute(GravityPlayerComponent* player) override;
};
