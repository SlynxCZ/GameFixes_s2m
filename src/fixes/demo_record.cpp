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

#include "demo_record.h"

#include <engine2/gamesessionconfiguration.h>

#include <cstdio>

const char* CDemoRecordFix::GetDescription() const
{
    return "With tv_enable 1: a kick of the GOTV client is held back while somebody is on the server (a shutdown and\n"
           "a kick on an empty server go through), and its slot is reserved on top of the session's maxplayers, so\n"
           "the demo keeps recording across match restarts on the same map.";
}

bool CDemoRecordFix::Load(char* error, size_t maxlen)
{
    ISource2ModFunction* pFunction = FindFunction("CServerSideClientBase::Disconnect", error, maxlen);
    if (!pFunction)
    {
        std::snprintf(error, maxlen, "CServerSideClientBase::Disconnect not found (see the console)");
        return false;
    }

    if (!Hook(HookPoint::Function(pFunction), HookTarget::All(), { this, &CDemoRecordFix::OnDisconnect }))
    {
        std::snprintf(error, maxlen, "could not hook CServerSideClient::Disconnect");
        return false;
    }

    m_nTvEnable = CVAR_IDX("tv_enable");

    Log("hooked CServerSideClient::Disconnect, tv_enable is %s",
        m_nTvEnable == SOURCE2MOD_INVALID_CONVAR_INDEX ? "not found" : (IsTvEnabled() ? "on" : "off"));
    return true;
}

void CDemoRecordFix::Unload()
{
    // The hook and its function are taken down by the plugin.
    m_nTvEnable = SOURCE2MOD_INVALID_CONVAR_INDEX;
}

bool CDemoRecordFix::IsTvEnabled()
{
    if (m_nTvEnable == SOURCE2MOD_INVALID_CONVAR_INDEX)
        m_nTvEnable = CVAR_IDX("tv_enable");

    return m_nTvEnable != SOURCE2MOD_INVALID_CONVAR_INDEX && CVAR_GET_BOOL(m_nTvEnable);
}

void CDemoRecordFix::OnStartupServer(StartupServerContext& ctx)
{
    if (m_nBlocked > 1)
        Log("%u GOTV kicks blocked on the last map", m_nBlocked);

    m_nBlocked = 0;
    m_bLoggedReleased = false;

    const bool bTvEnabled = IsTvEnabled();
    const uint32 nMaxClients = ctx.config->max_clients();

    // tv_enable is read right now: a server.cfg another plugin runs after the map has
    // started has not set it yet at the first map (see the log of the next map).
    Log("StartupServer %s: tv_enable is %d, session max_clients %u%s", ctx.mapName ? ctx.mapName : "?", bTvEnabled ? 1 : 0, nMaxClients,
        bTvEnabled ? "" : " (left as it is)");

    if (!bTvEnabled)
        return;

    // The engine reads max_clients after this Pre hook, so the GOTV slot comes on top of the configured player count instead of eating one.
    Log("GOTV is enabled, expanding the session's maxplayers (%u -> %u)", nMaxClients, nMaxClients + 1);
    const_cast<GameSessionConfiguration_t&>(*ctx.config).set_max_clients(nMaxClients + 1);
}

bool CDemoRecordFix::AnyHumanConnected() const
{
    CGlobalVars* pGlobals = g_pEngineServer->GetServerGlobals();
    if (!pGlobals)
        return true;

    for (int i = 0; i < pGlobals->maxClients; i++)
    {
        const CPlayerSlot slot(i);
        if (!g_pSource2ModClients->IsConnected(slot))
            continue;

        // The GOTV client and the bots are clients too, but nobody to record for.
        const auto* pClient = reinterpret_cast<const CServerSideClientBase*>(g_pSource2ModClients->GetServerSideClient(slot));
        if (pClient && !pClient->IsFakeClient() && !pClient->m_bIsHLTV)
            return true;
    }

    return false;
}

HookReturn<void> CDemoRecordFix::OnDisconnect(FunctionContext& ctx, bool post)
{
    GF_VPROF("GameFixes::demo_record::Disconnect");

    const auto* pThis = ctx.GetArg<const CServerSideClientBase*>(0);
    if (!pThis || !pThis->m_bIsHLTV)
        return Action::Ignore;

    const int nReason = ctx.GetArg<int>(1);
    const char* pszInternalReason = ctx.GetArg<const char*>(2);

    if (!IsTvEnabled())
    {
        // Something turned tv_enable off before the GOTV client went: let it go.
        Log("GOTV client disconnecting with tv_enable 0, not blocked (reason %d, \"%s\")", nReason,
            pszInternalReason ? pszInternalReason : "");
        return Action::Ignore;
    }

    // Only a kick is held back -- what a match restart does to the GOTV client, or bot_quota
    // taking it for a bot. A shutdown, HLTVSTOP, the server exiting and the like are the
    // engine tearing the client down, and it has to be able to finish that.
    if (nReason != NETWORK_DISCONNECT_KICKED)
        return Action::Ignore;

    // Nobody left to record for: the engine kicks the GOTV client again and again, and
    // holding it back only keeps that going. It is let go and comes back with the next match.
    if (!AnyHumanConnected())
    {
        if (!m_bLoggedReleased)
        {
            m_bLoggedReleased = true;
            Log("no player on the server, the GOTV client's kick is not blocked (reason %d, \"%s\")", nReason,
                pszInternalReason ? pszInternalReason : "");
        }
        return Action::Ignore;
    }

    m_nBlocked++;
    if (m_nBlocked == 1)
    {
        Log("GOTV is enabled, blocking the GOTV client's kick (tv_enable is 1, reason %d, \"%s\"); the next ones on this map are only counted",
            nReason, pszInternalReason ? pszInternalReason : "");
    }

    return Action::Supersede;
}
