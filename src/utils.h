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
#pragma once

// Small helpers for the fixes. Nothing here owns state a fix has to manage.

#include <source2mod/ISource2ModModule.h>    // WIN_LINUX(win, linux)

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * The plugin's log line: "[Game Fixes] <message>" on the server console, a
 * newline added. A fix logs through CFix::Log() instead, which prefixes its
 * name.
 */
void GF_Log(const char* pszFormat, ...);

/**
 * Game time in seconds, as a double.
 *
 * What a fix measures a delay or an interval against -- never std::chrono's
 * steady_clock: the game pauses, hibernates and changes map, and a wall clock
 * knows nothing of it. It is the engine's curtime summed frame by frame (the
 * same algorithm as Source2Mod's own scheduler, so it agrees with its timers,
 * though it is not the same counter): it never goes back on a map change
 * (curtime restarts at 0 there), and it keeps its precision where curtime, a
 * 32-bit float, does not after a day or two on one map. A frame the server
 * is not simulating (empty and hibernating) counts as 15 ms.
 *
 * Starts at 0 when the plugin loads; only advances while at least one fix is
 * loaded (the plugin's GameFrame hook ticks it).
 */
double GameTime();

/// The engine's raw curtime (float, restarts on every map); 0 before the first map.
float CurTime();

/// Plugin only: advances GameTime(); called once per GameFrame, before the fixes' OnGameFrame.
void TickGameTime(bool simulating);
/// Plugin only: back to 0 (plugin load).
void ResetGameTime();

/**
 * Overwrites @p len bytes of code at @p address (a patch). The page is made
 * writable for the copy and goes back to read + execute, on both platforms.
 * Source2Mod has no patch facility to prefer: GAMECONFIG_PATCH only returns
 * the gamedata text. Resolve the address with GAMECONFIG_RESOLVE(name) or
 * ISource2ModModule::FindPattern, keep the original bytes yourself and write
 * them back in Unload().
 *
 * @return false when @p address is null or the page could not be changed.
 */
bool WriteCode(void* address, const void* pBytes, std::size_t len);

/**
 * "90 90 EB" -> {0x90, 0x90, 0xEB}: the bytes of a gamedata "patches" entry
 * (GAMECONFIG_PATCH(name) is just that entry's text for this platform). Spaces
 * are optional; anything that is not a hex pair gives an empty vector.
 */
std::vector<uint8_t> ParseHexBytes(const char* pszText);
