#include "Entity.h"

#include "../Memory.h"
#include "../Interfaces.h"
#include "GlobalVars.h"
#include "Localize.h"

#include "Engine.h"
#include "LocalPlayer.h"

int Entity::getUserId() noexcept
{
    if (PlayerInfo playerInfo; interfaces->engine->getPlayerInfo(index(), playerInfo))
        return playerInfo.userId;
    return -1;
}

std::uint64_t Entity::getSteamId() noexcept
{
    if (PlayerInfo playerInfo; interfaces->engine->getPlayerInfo(index(), playerInfo))
        return playerInfo.xuid;
    return 0;
}
