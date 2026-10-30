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

#include "slow_animation.h"

#include <source2mod/sdk/schema/classes/CBasePlayerController.h>

#include <gameentitysystem.h>

#include <cstdio>

void CSlowAnimationFix::ReadConfig(const FixConfig& config)
{
    m_flReloadInterval = config.GetFloat("reload_interval", 1800.0f);

    // Anything shorter would just keep flipping the map around.
    if (m_flReloadInterval < 60.0f)
        m_flReloadInterval = 60.0f;
}

bool CSlowAnimationFix::Load(char* error, size_t maxlen)
{
    if (!g_pSource2ModMaps)
    {
        snprintf(error, maxlen, "the core has no ISource2ModMaps (update Source2Mod)");
        return false;
    }

    // Game time, on the scheduler's repeating timer: survives map changes, dropped by the plugin on unload.
    if (!AddRepeatingTimer(m_flReloadInterval, [this]() { OnReloadTimer(); }))
    {
        snprintf(error, maxlen, "could not create the reload timer");
        return false;
    }

    Log("checking for an empty server every %.0f s", m_flReloadInterval);
    return true;
}

void CSlowAnimationFix::OnStartupServer(StartupServerContext& ctx)
{
    snprintf(m_szMap, sizeof(m_szMap), "%s", (ctx.mapName && ctx.mapName[0]) ? ctx.mapName : "unknown");
    m_dMapStartTime = GameTime();

    Log("map snapshot '%s'", m_szMap);

    if (m_flPendingTimelimit < 0.0f)
        return;

    const float flRemaining = m_flPendingTimelimit;
    m_flPendingTimelimit = -1.0f;

    // The map's own cfg runs after StartupServer and would overwrite the value; one frame later is past that.
    g_pSource2ModScheduler->NextFrame([this, flRemaining]()
    {
        const float flValue = flRemaining > 0.0f ? flRemaining : 0.1f;
        if (!UTIL_SetConVar("mp_timelimit", flValue))
            return;

        Log("restoring the remaining timelimit -> mp_timelimit %.1f", flValue);
    });
}

void CSlowAnimationFix::OnReloadTimer()
{
    GF_VPROF("GameFixes::slow_animation::OnReloadTimer");

    if (!m_szMap[0])
    {
        Log("reload skipped: no map snapshot yet (loaded mid-map, waiting for the next StartupServer)");
        return;
    }

    const int iPlayers = CountHumanPlayers();
    if (iPlayers > 0)
    {
        Log("reload skipped: %d human player(s) on '%s', next check in %.0f s", iPlayers, m_szMap, m_flReloadInterval);
        return;
    }

    const float flElapsedMinutes = static_cast<float>(GameTime() - m_dMapStartTime) / 60.0f;
    const uint16 iTimelimit = CVAR_IDX("mp_timelimit");
    const float flTimelimit = iTimelimit != SOURCE2MOD_INVALID_CONVAR_INDEX ? CVAR_GET_FLOAT(iTimelimit) : 0.0f;

    if (flTimelimit > 0.0f)
    {
        m_flPendingTimelimit = flTimelimit - flElapsedMinutes;
        Log("timelimit %.1f min, elapsed %.1f min -> restoring %.1f min after the reload", flTimelimit, flElapsedMinutes, m_flPendingTimelimit);
    }

    // ISource2ModMaps does what mm did by hand: a map the server has goes
    // through `changelevel`, anything else (a Workshop map) through
    // `ds_workshop_changelevel` / `host_workshop_map`.
    Log("server empty -> ChangeLevel('%s')", m_szMap);
    if (!g_pSource2ModMaps->ChangeLevel(m_szMap, "Game Fixes: slow_animation, empty server"))
    {
        Log("reload failed: the core refused the map name '%s'", m_szMap);
        m_flPendingTimelimit = -1.0f;
    }
}

int CSlowAnimationFix::CountHumanPlayers() const
{
    CGameEntitySystem* pEntitySystem = GameEntitySystem();
    CGlobalVars* pGlobals = g_pEngineServer->GetServerGlobals();
    if (!pEntitySystem || !pGlobals)
        return 0;

    int iPlayers = 0;
    for (int i = 0; i < pGlobals->maxClients; i++)
    {
        auto* pController = static_cast<CBasePlayerController*>(pEntitySystem->GetEntityInstance(CEntityIndex(i + 1)));
        if (pController && pController->IsConnected() && !pController->IsBot() && !pController->m_bIsHLTV)
            iPlayers++;
    }

    return iPlayers;
}
