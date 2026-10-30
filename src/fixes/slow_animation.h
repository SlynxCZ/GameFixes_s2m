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

// curtime is a 32-bit float; after a day or two on one map it has lost enough
// precision for animation, movement and lag compensation to go sluggish, and
// nothing in the game ever resets it. Every reload_interval seconds, while no
// human is connected, the current map is reloaded; the remaining mp_timelimit
// is carried over to the fresh map.
#pragma once

#include "fix.h"

class CSlowAnimationFix final : public CFix
{
public:
    const char* GetName() const override { return "slow_animation"; }
    const char* GetDescription() const override
    {
        return "curtime is a 32-bit float and loses precision after a day or two of\n"
               "uptime on one map (animations and movement turn sluggish). Every\n"
               "reload_interval seconds, while no human is connected, the current map\n"
               "is reloaded to reset it; the remaining mp_timelimit is carried over.";
    }
    const char* GetConfigDefaults() const override
    {
        return R"json(
        "reload_interval": 1800   // seconds between empty-server checks (at least 60)
)json";
    }

    void ReadConfig(const FixConfig& config) override;
    bool Load(char* error, size_t maxlen) override;
    void OnStartupServer(StartupServerContext& ctx) override;

private:
    void OnReloadTimer();
    int CountHumanPlayers() const;

    // Seconds between empty-server checks ("reload_interval").
    float m_flReloadInterval = 1800.0f;

    // Map name and game time from the last StartupServer.
    char m_szMap[256] = "";
    double m_dMapStartTime = 0.0;

    // Remaining mp_timelimit (minutes) to restore after a reload; < 0 = none.
    float m_flPendingTimelimit = -1.0f;
};
