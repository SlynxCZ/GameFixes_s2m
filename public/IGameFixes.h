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
#ifndef _INCLUDE_GAMEFIXES_IFACE_H_
#define _INCLUDE_GAMEFIXES_IFACE_H_
#ifdef _WIN32
#pragma once
#endif

#include <cstdint>

#define GAMEFIXES_INTERFACE_VERSION "IGameFixes002"
// The first revision (steam_ban only) is the same object with the first two
// methods below: a plugin built against it keeps working.
#define GAMEFIXES_INTERFACE_VERSION_001 "IGameFixes001"

// What other plugins can ask of GameFixes. From a Source2Mod plugin, once every
// plugin is loaded (OnAllSource2ModPluginsLoaded), treating null as "no GameFixes":
//
//   IGameFixes* g_pGameFixes = nullptr;
//   int ret;
//   GET_SOURCE2MOD_IFACE(g_pGameFixes, IGameFixes, GAMEFIXES_INTERFACE_VERSION, ret);
//
// A Metamod:Source plugin gets it through g_SMAPI->MetaFactory(GAMEFIXES_INTERFACE_VERSION, ...).
//
// A call to a fix that is off (or failed to load) does nothing and returns false.
class IGameFixes
{
public:
	// steam_ban: the engine's GC ban / competitive cooldown kick (game bans,
	// VAC Live, cooldowns) leaves this account alone, like the whitelist in
	// game_fixes.json, until DisallowAccount or a plugin reload. An account the
	// engine wants kicked is held for "hold_seconds" first, so a plugin that
	// learns who the player is a moment after they connect (an admin list over
	// HTTP) can still allow them in time.
	virtual bool SteamBanAllowAccount(uint64_t steamId64) = 0;
	virtual bool SteamBanDisallowAccount(uint64_t steamId64) = 0;

	// server_list_players: the account is left out of what is pushed to Steam's
	// server list (a hidden admin, a spectator bot, ...) from the next push on,
	// until ServerListPlayersShow or a plugin reload. Steam keeps what it was
	// last given for a player until they disconnect, so hide an account before
	// its first push (when it connects) rather than after.
	virtual bool ServerListPlayersHide(uint64_t steamId64) = 0;
	virtual bool ServerListPlayersShow(uint64_t steamId64) = 0;
};

#endif // _INCLUDE_GAMEFIXES_IFACE_H_
