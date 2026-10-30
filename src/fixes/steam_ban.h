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

// The engine's per-frame GC ban / competitive cooldown kick pass
// (GameSystem_Think_CheckSteamBan) walks CCSGameRules::sm_mapGcBanInformation
// and kicks every connected account it finds there. Hooked by signature, the
// way CS2Fixes does it: Pre strips the whitelisted accounts out of the map and,
// below sv_kick_players_with_cooldown 2, the competitive-cooldown entries the
// pass would still act on; Post clears whatever the pass left in it, so a
// stale entry can't spread to the next player to join.
//
// Besides the whitelist, other plugins allow accounts at runtime through
// IGameFixes (FUNPLAY: its active admins). An account the pass would act on
// that nobody allowed yet is held for "hold_seconds" from when it first shows
// up in the map: the pass is skipped while one is held -- the entry can't be
// taken out and put back, the map only allows structural access -- and runs as
// usual once the hold is over.
#pragma once

#include "fix.h"
#include "sdk/game/server/gcbaninformation.h"

#include <tier0/utlmap.h>

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

class CSteamBanFix final : public CFix, public ISteamBanControl
{
public:
    const char* GetName() const override { return "steam_ban"; }
    const char* GetDescription() const override;
    const char* GetConfigDefaults() const override;

    void ReadConfig(const FixConfig& config) override;
    bool Load(char* error, size_t maxlen) override;
    void Unload() override;

    // ISteamBanControl (IGameFixes): accounts allowed at runtime, on top of the whitelist
    bool AllowAccount(uint64_t steamId64) override;
    bool DisallowAccount(uint64_t steamId64) override;

private:
    // GameSystem_Think_CheckSteamBan, Pre and Post.
    HookReturn<void> OnCheckSteamBan(FunctionContext& ctx, bool post);
    void BeforePass();
    void AfterPass();

    // Steam32 account ids ("whitelist", given as SteamID64s) the pass must
    // never see.
    std::unordered_set<uint32> m_whitelist;

    // "clear_after_pass": empty the map once the pass ran, so an entry the
    // engine already acted on never reaches the next player to join.
    bool m_bClearAfterPass = true;

    // Accounts other plugins allowed (IGameFixes), Steam32 account ids
    std::unordered_set<uint32> m_allowed;

    // "hold_seconds": how long an account no one allowed yet is kept from the
    // pass after it first shows up in the map; 0 turns holding off. Measured
    // in GameTime().
    double m_flHoldTime = 15.0;
    std::unordered_map<uint32, double> m_firstSeen;

    // The pass was skipped this frame: its entries stay for the next one
    bool m_bPassSkipped = false;

    // sv_kick_players_with_cooldown; resolved on first use if the convar is not there yet at Load
    uint16 m_nCooldownConVar = SOURCE2MOD_INVALID_CONVAR_INDEX;

    // CCSGameRules::sm_mapGcBanInformation, keyed by account id. The third
    // template argument is deliberately uint32, not a real comparator, to
    // match the engine's layout -- so only structural, index-based access
    // (FOR_EACH_MAP / Key / Element / RemoveAt / RemoveAll) is valid on it,
    // never Find / Insert.
    CUtlOrderedMap<uint32, CGcBanInformation_t, uint32>* m_pBanMap = nullptr;
};
