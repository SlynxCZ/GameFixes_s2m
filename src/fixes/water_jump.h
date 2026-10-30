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

// Touching the bottom of a body of water, the CategorizePosition that follows
// WaterMove sees ground under the player and treats them as standing on it,
// and a jump out of the water no longer takes. That one CategorizePosition is
// skipped after every WaterMove, so the player stays in the water state and
// can jump out. Moved here from the FUNPLAY gamemode plugins.
#pragma once

#include "fix.h"

class CWaterJumpFix final : public CFix
{
public:
    const char* GetName() const override { return "water_jump"; }
    const char* GetDescription() const override
    {
        return "Touching the bottom of a body of water, the CategorizePosition after\n"
               "WaterMove counts the player as standing on it and a jump out of the\n"
               "water no longer takes. That one CategorizePosition is skipped.";
    }

    bool Load(char* error, size_t maxlen) override;
    void Unload() override;
    void OnStartupServer(StartupServerContext& ctx) override;

private:
    HookReturn<bool> OnWaterMove(MovementContext& ctx, bool post);
    HookReturn<void> OnCategorizePosition(CategorizePositionContext& ctx, bool post);

    // Which slot a movement service belongs to; -1 for anything that is not a player.
    static int GetSlot(CCSPlayer_MovementServices* pServices);

    // Per slot: WaterMove ran, the next CategorizePosition is to be skipped.
    bool m_bSkipNextCategorize[64] = {};
};
