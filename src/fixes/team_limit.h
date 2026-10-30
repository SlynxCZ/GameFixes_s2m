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

// A join is refused as "team full" once a team has as many players as the
// smaller of CCSGameRules' m_iMaxNum<team> and m_iSpawnPointCount_<team>, the
// number of enabled spawn points for it. The game sets the first at level init
// and recounts the second whenever a spawn point is created, enabled or
// disabled -- maps do that mid-round -- so both are raised to maxplayers every
// frame, when they are below it. Players beyond the map's spawn points share
// them. m_iNumSpawnable<team> are live player counts the game balances teams
// with, and are left alone.
#pragma once

#include "fix.h"

class CTeamLimitFix final : public CFix
{
public:
    const char* GetName() const override { return "team_limit"; }
    const char* GetDescription() const override
    {
        return "The game refuses a join as \"team full\" at the smaller of the team's max\n"
               "count and its enabled spawn points. Both are raised to maxplayers every\n"
               "frame (maps toggle spawn points mid-round), so players beyond the map's\n"
               "spawns share them. Autobalance is untouched; it only runs on a round\n"
               "restart, so servers with one long round never balance.";
    }

    bool Load(char* error, size_t maxlen) override { return true; }

    void OnGameFrame(bool simulating) override;
};
