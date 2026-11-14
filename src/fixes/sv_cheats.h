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

// Turning sv_cheats off leaves anyone who got noclip from the engine's cheat
// command flying around with it. Only that noclip is undone, never noclip or
// fly a plugin set (FUNPLAY's /noclip and /fly, JailBreak ghosts, ...):
//
// - A post listener on the engine "noclip" command notes the player while
//   sv_cheats is on. The core runs post listeners only when no pre listener
//   superseded the command, so a plugin that answers "noclip" itself (FUNPLAY
//   Core does, for every player) never gets a pawn noted.
// - On the next frame the pawn counts as in the cheat noclip only if the
//   command really took it from another move type to MOVETYPE_NOCLIP. The
//   actual move type it got is remembered with it.
// - Every frame a remembered pawn that is gone or out of noclip is forgotten,
//   so a plugin switching it off and on again takes it over.
// - When sv_cheats flips to 0, the remembered pawns still in that very noclip
//   go back to MOVETYPE_WALK: m_MoveType (networked) and m_nActualMoveType
//   both, the way the engine's own noclip toggle leaves them, and the noclip
//   speed is dropped so nobody is flung off.
#pragma once

#include "fix.h"

#include <ehandle.h>

#include <vector>

class CSvCheatsFix final : public CFix
{
public:
    const char* GetName() const override { return "sv_cheats"; }
    const char* GetDescription() const override
    {
        return "Turning sv_cheats off leaves anyone in the cheat noclip flying. Puts every\n"
               "pawn the engine's noclip command put in noclip (while sv_cheats was on)\n"
               "back to MOVETYPE_WALK the moment the cvar flips to 0. Noclip or fly set\n"
               "by a plugin is left alone.";
    }

    bool Load(char* error, size_t maxlen) override;
    void Unload() override;
    void OnGameFrame(bool simulating) override;

private:
    /// The engine's noclip command went through for this pawn; settled next frame.
    struct PendingNoclip
    {
        CHandle<CBaseEntity> hPawn;
        MoveType_t nMoveTypeBefore;
    };

    /// A pawn the cheat command put in noclip, with the actual move type it got from it.
    struct CheatNoclip
    {
        CHandle<CBaseEntity> hPawn;
        MoveType_t nActualMoveType;
    };

    bool CheatsOn() const;
    void OnNoclipCommand(CPlayerSlot slot);
    void OnSvCheatsDisabled();

    Source2ModCommandHookId m_noclipHookId = 0;
    uint16 m_svCheatsIndex = SOURCE2MOD_INVALID_CONVAR_INDEX;
    std::vector<PendingNoclip> m_pending;
    std::vector<CheatNoclip> m_tracked;
};
