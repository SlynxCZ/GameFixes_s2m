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

#include "steam_ban.h"

#include <steam/steamclientpublic.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

const char* CSteamBanFix::GetDescription() const
{
    return "The engine's per-frame GC ban / competitive cooldown kick pass (GameSystem_Think_CheckSteamBan).\n"
           "Accounts in whitelist are stripped out before the pass runs, so are competitive cooldowns while\n"
           "sv_kick_players_with_cooldown is below 2, and with clear_after_pass whatever the pass leaves behind\n"
           "is cleared so a stale entry can't hit the next player to join.";
}

const char* CSteamBanFix::GetConfigDefaults() const
{
    return R"json(
        // empty the ban map after the pass ran, so an entry the engine already
        // acted on never reaches the next player to join
        "clear_after_pass": true,
        // Other plugins can allow accounts at runtime (IGameFixes, e.g.
        // FUNPLAY's active admins). An account nobody allowed yet is held
        // this many (game) seconds before the engine may kick it; 0 = no hold.
        "hold_seconds": 15,
        // any label -> SteamID64, e.g. "Slynx": "76561198000000000"
        "whitelist": { }
)json";
}

void CSteamBanFix::ReadConfig(const FixConfig& config)
{
    m_bClearAfterPass = config.GetBool("clear_after_pass", true);
    m_flHoldTime = (std::max)(config.GetDouble("hold_seconds", 15.0), 0.0);

    for (const auto& [label, steamId64] : config.GetStringMap("whitelist"))
    {
        const CSteamID steamId((std::strtoull(steamId64.c_str(), nullptr, 10)));

        if (!steamId.IsValid())
        {
            Log("ignoring whitelist entry \"%s\" \"%s\": not a SteamID64", label.c_str(), steamId64.c_str());
            continue;
        }

        m_whitelist.insert(steamId.GetAccountID());
    }
}

bool CSteamBanFix::Load(char* error, size_t maxlen)
{
    // After https://github.com/Source2ZE/CS2Fixes/commit/c82d21ae36588520391b301ed02bfb851dff18e1,
    // plus the whitelist.

    // lea reg, [rip + sm_mapGcBanInformation] inside CCSGameRules; the map
    // is the rip-relative operand (both platforms: opcode, modrm, rel32 at +3, 7 bytes long).
    // Location to CUtlMap unk that is referenced on Windows by function with "Notification about user penalty: %u/%u (%u sec)\n" string
    // On Linux, a qword appears twice in GameSystem_Think_CheckSteamBan, and thrice in a sub-function of the function used for Windows (1 top, 2 bottom), the only other reference to this qword is some convar registration function with two unks above, sm_mapGcBanInformation is the unk further away
    const ISource2ModMemory pMapRef = FindAddress("CCSGameRules::sm_mapGcBanInformation", error, maxlen);
    if (!pMapRef)
        return false;

    m_pBanMap = pMapRef.ResolveRelativeAddress(3, 7).CCast<decltype(m_pBanMap)>();

    // void GameSystem_Think_CheckSteamBan()
    ISource2ModFunction* pCheckSteamBan = FindFunction("GameSystem_Think_CheckSteamBan", error, maxlen);
    if (!pCheckSteamBan)
        return false;

    const HookPointOf<FunctionContext> point = HookPoint::Function(pCheckSteamBan);
    if (!Hook(point, HookTarget::All(), { this, &CSteamBanFix::OnCheckSteamBan }, false) ||
        !Hook(point, HookTarget::All(), { this, &CSteamBanFix::OnCheckSteamBan }, true))
    {
        std::snprintf(error, maxlen, "could not hook GameSystem_Think_CheckSteamBan");
        return false;
    }

    m_nCooldownConVar = CVAR_IDX("sv_kick_players_with_cooldown");

    Log("hooked GameSystem_Think_CheckSteamBan, ban map at %p, %zu whitelisted account(s), clear after pass %s, hold %.0f s",
        static_cast<void*>(m_pBanMap), m_whitelist.size(), m_bClearAfterPass ? "on" : "off", m_flHoldTime);
    return true;
}

void CSteamBanFix::Unload()
{
    // The hooks are taken down by the plugin.
    m_pBanMap = nullptr;
    m_firstSeen.clear();
    m_bPassSkipped = false;
}

bool CSteamBanFix::AllowAccount(uint64_t steamId64)
{
    const CSteamID steamId(static_cast<uint64>(steamId64));
    if (!steamId.IsValid())
        return false;

    m_allowed.insert(steamId.GetAccountID());
    return true;
}

bool CSteamBanFix::DisallowAccount(uint64_t steamId64)
{
    const CSteamID steamId(static_cast<uint64>(steamId64));
    return steamId.IsValid() && m_allowed.erase(steamId.GetAccountID()) > 0;
}

HookReturn<void> CSteamBanFix::OnCheckSteamBan(FunctionContext& ctx, bool post)
{
    if (post)
    {
        GF_VPROF("GameFixes::steam_ban::CheckSteamBanPost");
        AfterPass();
        return Action::Ignore;
    }

    GF_VPROF("GameFixes::steam_ban::CheckSteamBan");
    BeforePass();
    return m_bPassSkipped ? Action::Supersede : Action::Ignore;
}

void CSteamBanFix::BeforePass()
{
    m_bPassSkipped = false;

    if (!m_pBanMap || m_pBanMap->Count() <= 0)
    {
        m_firstSeen.clear();
        return;
    }

    // Below sv_kick_players_with_cooldown 2 the pass still acts on competitive
    // cooldowns (reasons 20, 22 and 23); those entries go the same way as the
    // whitelisted accounts, before the pass gets to see them.
    if (m_nCooldownConVar == SOURCE2MOD_INVALID_CONVAR_INDEX)
        m_nCooldownConVar = CVAR_IDX("sv_kick_players_with_cooldown");
    const bool bDropCooldowns = m_nCooldownConVar != SOURCE2MOD_INVALID_CONVAR_INDEX && CVAR_GET_INT(m_nCooldownConVar) < 2;

    // Collect first, remove after -- mutating the tree mid-walk isn't safe.
    std::vector<int> toRemove;
    FOR_EACH_MAP(*m_pBanMap, i)
    {
        const uint32 uReason = m_pBanMap->Element(i).m_uiReason;

        if (m_whitelist.contains(m_pBanMap->Key(i)) || m_allowed.contains(m_pBanMap->Key(i)))
            toRemove.push_back(i);
        else if (bDropCooldowns && (uReason == 20 || uReason == 22 || uReason == 23))
            toRemove.push_back(i);
    }

    for (int i : toRemove)
        m_pBanMap->RemoveAt(i);

    if (m_flHoldTime <= 0.0)
        return;

    // What is left would be kicked: hold it while it's new, in case a plugin allows it in time
    const double now = GameTime();
    bool bHold = false;
    std::unordered_set<uint32> inMap;
    FOR_EACH_MAP(*m_pBanMap, i)
    {
        const uint32 uAccount = m_pBanMap->Key(i);
        inMap.insert(uAccount);
        const double firstSeen = m_firstSeen.try_emplace(uAccount, now).first->second;
        if (now - firstSeen < m_flHoldTime)
            bHold = true;
    }
    std::erase_if(m_firstSeen, [&inMap](const auto& entry) { return !inMap.contains(entry.first); });

    m_bPassSkipped = bHold;
}

void CSteamBanFix::AfterPass()
{
    // Skipped while an account is held: its entries wait for the next frame
    if (m_bPassSkipped)
    {
        m_bPassSkipped = false;
        return;
    }

    // Whoever the pass wanted kicked has been kicked by now; the rest of the map
    // is stale and must not survive to the next frame. (Shared by @aiolos1045.)
    if (m_bClearAfterPass && m_pBanMap && m_pBanMap->Count() > 0)
        m_pBanMap->RemoveAll();
}
