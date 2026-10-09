#include "Input/PlayerCommands.h"
#include "Player/GravityPlayerComponent.h"

void PullDebrisCommand::Execute(GravityPlayerComponent* player) {
    if (player) {
        player->ExecutePullAction();
    }
}

void ThrowDebrisCommand::Execute(GravityPlayerComponent* player) {
    if (player) {
        player->ExecuteThrowAction();
    }
}

void MarkTargetCommand::Execute(GravityPlayerComponent* player) {
    if (player) {
        player->ExecuteMarkAction();
    }
}
