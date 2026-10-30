/**
 * vim: set ts=4 sw=4 tw=99 noet:
 * =============================================================================
 * GameFixes_s2m
 * Copyright (C) 2026 Michal "Slynx (˙·٠● S l y n x ●٠·˙)" Přikryl.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, Michal "Slynx (˙·٠● S l y n x ●٠·˙)" Přikryl gives
 * you permission to link the code of this program (as well as its derivative
 * works) to "Counter-Strike 2," "Source 2," "Steam," and any Game MODs or
 * server software running on software by Valve Corporation. You must obey the
 * GNU General Public License in all respects for all other code used.
 *
 * Additionally, this exception applies to all derivative works unless
 * otherwise stated in LICENSE.txt.
 *
 * Authors:
 *   - Michal "Slynx (˙·٠● S l y n x ●٠·˙)" Přikryl
 *
 * Project: GameFixes_s2m
 */

#include "server_list_players.h"

#include <cstdlib>

#include <source2mod/sdk/schema/classes/CBasePlayerController.h>

#include <gameentitysystem.h>
#include <steam/steam_gameserver.h>

void CServerListPlayersFix::ReadConfig(const FixConfig& config)
{
    m_flUpdateInterval = config.GetFloat("update_interval", 5.0f);

    if (m_flUpdateInterval < 1.0f)
        m_flUpdateInterval = 1.0f;

    m_ConfigHidden.clear();
    for (const auto& [label, id] : config.GetStringMap("hide"))
    {
        const uint64_t steamId64 = std::strtoull(id.c_str(), nullptr, 10);
        if (steamId64)
            m_ConfigHidden.insert(steamId64);
    }

    m_Hidden = m_ConfigHidden;
}

bool CServerListPlayersFix::Load(char* error, size_t maxlen)
{
    // After the game's own handler, so SteamGameServer() already answers.
    const Source2ModHookId activated = Hook(HookPoint::GameServerSteamAPIActivated, HookTarget::All(),
        [this](EmptyContext&, bool post) -> HookReturn<void>
        {
            GF_VPROF("GameFixes::server_list_players::SteamAPIActivated");

            m_bSteamAPIReady = SteamGameServer() != nullptr;
            return Action::Ignore;
        }, true);

    const Source2ModHookId deactivated = Hook(HookPoint::GameServerSteamAPIDeactivated, HookTarget::All(),
        [this](EmptyContext&, bool post) -> HookReturn<void>
        {
            GF_VPROF("GameFixes::server_list_players::SteamAPIDeactivated");

            m_bSteamAPIReady = false;
            return Action::Ignore;
        }, false);

    if (!activated || !deactivated)
    {
        snprintf(error, maxlen, "the Steam API activation hook points are not available on this game");
        return false;
    }

    // Loaded after the API already came up (late load): there won't be another activation, so try it right away.
    m_bSteamAPIReady = SteamGameServer() != nullptr;

    if (!AddRepeatingTimer(m_flUpdateInterval, [this]() { UpdatePlayers(); }))
    {
        snprintf(error, maxlen, "could not create the update timer");
        return false;
    }

    Log("pushing player data to Steam every %.0f s (Steam API %s)", m_flUpdateInterval, m_bSteamAPIReady ? "ready" : "not up yet");
    return true;
}

void CServerListPlayersFix::Unload()
{
    m_bSteamAPIReady = false;
    m_Hidden.clear();
}

bool CServerListPlayersFix::HidePlayer(uint64_t steamId64)
{
    return steamId64 != 0 && m_Hidden.insert(steamId64).second;
}

bool CServerListPlayersFix::ShowPlayer(uint64_t steamId64)
{
    // What the config hides stays hidden: it is the server's rule, a plugin
    // only takes back what it hid itself.
    if (m_ConfigHidden.count(steamId64))
        return false;

    return m_Hidden.erase(steamId64) != 0;
}

void CServerListPlayersFix::UpdatePlayers()
{
    GF_VPROF("GameFixes::server_list_players::UpdatePlayers");

    if (!m_bSteamAPIReady)
        return;

    ISteamGameServer* pGameServer = SteamGameServer();
    CGameEntitySystem* pEntitySystem = GameEntitySystem();
    CGlobalVars* pGlobals = g_pEngineServer->GetServerGlobals();
    if (!pGameServer || !pEntitySystem || !pGlobals)
        return;

    for (int i = 0; i < pGlobals->maxClients; i++)
    {
        const CPlayerSlot slot(i);
        if (!g_pSource2ModClients->IsConnected(slot))
            continue;

        const CSteamID* pSteamId = g_pEngineServer->GetClientSteamID(slot);
        if (!pSteamId)
            continue;

        if (!m_Hidden.empty() && m_Hidden.count(pSteamId->ConvertToUint64()))
            continue;

        auto* pController = static_cast<CBasePlayerController*>(pEntitySystem->GetEntityInstance(CEntityIndex(i + 1)));
        if (!pController)
            continue;

        pGameServer->BUpdateUserData(*pSteamId, pController->GetPlayerName(), g_pSource2GameClients->GetPlayerScore(slot));
    }
}
