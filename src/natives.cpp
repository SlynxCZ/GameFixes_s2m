/*
 * SourcePawn natives of gamefixes.inc: IGameFixes, by SteamID in any form scripts have it.
 */
#include "plugin.h"
#include "source2mod/ISource2ModNatives.h"
#include "gamefixes/IGameFixes.h"

#include <cstdlib>

using namespace SourcePawn;

namespace
{
    const char* Str(IPluginContext* ctx, cell_t addr)
    {
        char* s = nullptr;
        return ctx->LocalToString(addr, &s) == SP_ERROR_NONE && s ? s : "";
    }

    IGameFixes* Fixes()
    {
        return static_cast<IGameFixes*>(g_Plugin.OnSource2ModQuery(GAMEFIXES_INTERFACE_VERSION, nullptr));
    }

    uint64_t Id(IPluginContext* ctx, cell_t addr)
    {
        return std::strtoull(Str(ctx, addr), nullptr, 10);
    }

    template <bool (IGameFixes::*Call)(uint64_t)>
    cell_t N_Account(IPluginContext* ctx, const cell_t* params)
    {
        IGameFixes* fixes = Fixes();
        return fixes && (fixes->*Call)(Id(ctx, params[1]));
    }

    const sp_nativeinfo_t g_natives[] = {
        { "GameFixes.SteamBanAllowAccount", N_Account<&IGameFixes::SteamBanAllowAccount> },
        { "GameFixes.SteamBanDisallowAccount", N_Account<&IGameFixes::SteamBanDisallowAccount> },
        { "GameFixes.ServerListPlayersHide", N_Account<&IGameFixes::ServerListPlayersHide> },
        { "GameFixes.ServerListPlayersShow", N_Account<&IGameFixes::ServerListPlayersShow> },
        { nullptr, nullptr },
    };
}

void RegisterGameFixesNatives()
{
    g_pSource2ModNatives->AddNatives(g_natives);
    g_pSource2ModNatives->RegisterLibrary("GameFixes");
}
