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

// bot_quota "fill" picks a bot to remove when there are too many players, and
// the SourceTV client (a controller with no pawn) can pass for one: it is
// kicked, the demo being recorded stops and the next recording does not start
// normally. While the engine selects a bot to remove, this fix rejects the HLTV
// controller as a candidate, so the selection goes on to an ordinary bot.
// Bot settings, explicit kicks and map changes are left alone. (A port of
// SwiftlyS2-Plugins/Fixes' "CSTV removed by bot quota"; a server without bots
// does not need it, hence off by default.)
#pragma once

#include "fix.h"

class CCstvBotQuotaFix final : public CFix
{
public:
    const char* GetName() const override { return "cstv_bot_quota"; }
    const char* GetDescription() const override
    {
        return "Keeps the SourceTV client out of bot_quota's bot removal (bot_quota_mode fill),\n"
               "so automatic bot management no longer kicks it and stops the demo recording.\n"
               "Off by default: a server without bots does not need it.";
    }
    bool EnabledByDefault() const override { return false; }

    bool Load(char* error, size_t maxlen) override;
    void Unload() override;

    void OnStartupServer(StartupServerContext& ctx) override;

private:
    // The engine's "select a bot of this team and kick it", Pre and Post: the
    // scope in which the team predicate below is allowed to refuse the HLTV.
    HookReturn<void> OnSelectAndKickBot(FunctionContext& ctx, bool post);

    // The engine's team predicate (bool(int* team, controller, pawn)), shared by
    // a lot of unrelated code, so it only refuses inside the selection.
    HookReturn<void> OnTeamCandidate(FunctionContext& ctx, bool post);

    int m_nSelectionDepth = 0;
    bool m_bLoggedSkip = false;
};
