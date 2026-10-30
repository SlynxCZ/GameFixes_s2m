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

// Surf ramps: a movement trace that stops a hair short of a ramp, or lands on
// its edge, hands TryPlayerMove a plane the player never really hit, and the
// player is flung off -- a rampbug. Every trace of the pass is redone from a
// small offset along the last plane the player was known to be on, and a
// result the game then mangles is put back where it belongs; CategorizePosition
// gets the same treatment on the way down a ramp. CS2KZ's fix, as carried by
// CS2Fixes-RampbugFix (github.com/Interesting-exe/CS2Fixes-RampbugFix). It
// lessens rampbugs, it does not end them.
//
// The game's own TryPlayerMove now pushes the end point off a single hit plane
// (Valve's own mitigation), so putting our walk back over the game's
// ("tryplayermove_override") is off by default; the walk still runs, for the
// last valid plane CategorizePosition works with. What a player was on is
// forgotten when the game moves them outside the pass (a teleport).
#pragma once

#include "fix.h"

#include <source2mod/sdk/game/shared/movedata.h>

#include <mathlib/vector.h>

class CCSPlayerPawn;
class CTraceFilter;

class CRampbugFix final : public CFix
{
public:
    const char* GetName() const override { return "rampbug"; }
    const char* GetDescription() const override
    {
        return "Surf ramps: a trace that stops a hair short of a ramp, or on its edge,\n"
               "hands TryPlayerMove a plane it never really hit, and the player is\n"
               "flung off (a rampbug). The traces are redone from a small offset along\n"
               "the last valid plane, and a bad result is put back where it belongs.\n"
               "CS2KZ's fix, as carried by CS2Fixes-RampbugFix; it lessens rampbugs,\n"
               "it does not end them. What a player was on is forgotten on a teleport.";
    }
    const char* GetConfigDefaults() const override
    {
        return R"json(
    // Put the fix's own walk back over the game's TryPlayerMove when the
    // game moved the player elsewhere. The game has its own mitigation
    // now and this overrides far more often than needed: off by default.
    "tryplayermove_override": false
)json";
    }

    void ReadConfig(const FixConfig& config) override;
    bool Load(char* error, size_t maxlen) override;
    void Unload() override;
    void OnStartupServer(StartupServerContext& ctx) override;

private:
    // What a trace of the pass is reduced to: the only fields the walk reads.
    struct MoveTrace
    {
        Vector m_vStartPos = vec3_origin;
        Vector m_vEndPos = vec3_origin;
        Vector m_vHitNormal = vec3_origin;
        float m_flFraction = 1.0f;
        bool m_bStartInSolid = false;
    };

    // The player's bounds.
    struct PlayerBounds_t
    {
        Vector mins;
        Vector maxs;
    };

    // What one player's movement pass left behind, from one call to the next.
    struct PlayerState
    {
        CMoveData* pMoveData = nullptr;       // the pass in flight, null outside ProcessMovement
        bool bProcessingMovement = false;
        bool bPreviousOnGround = false;
        int nGameCodeMoves = 0;               // m_nGameCodeHasMovedPlayerAfterCommand last seen
        bool bDidTPM = false;                 // TryPlayerMove ran during this pass
        bool bOverrideTPM = false;            // our own trace result should replace the game's
        Vector vecTPMVelocity;                // vec3_invalid until TryPlayerMove ran
        Vector vecTPMOrigin;
        Vector vecLastValidPlane;             // vec3_origin when there is none
        Vector vecTakeoffVelocity;
        Vector vecLandingOrigin;
        Vector vecLandingVelocity;

        void Reset();
        // Forgets what the player was on, keeping the pass in flight
        void ForgetSurface();
    };

    HookReturn<bool> OnProcessMovement(MovementContext& ctx, bool post);
    HookReturn<void> OnTryPlayerMove(TryPlayerMoveContext& ctx, bool post);
    HookReturn<void> OnCategorizePosition(CategorizePositionContext& ctx, bool post);

    void ProcessMovementPre(CCSPlayer_MovementServices* pServices, CMoveData* pMove);
    void ProcessMovementPost(CCSPlayer_MovementServices* pServices);
    void TryPlayerMovePre(TryPlayerMoveContext& ctx);
    void TryPlayerMovePost(TryPlayerMoveContext& ctx);

    // "tryplayermove_override": put our own walk back when the game's moved the player elsewhere
    bool m_bOverrideTryPlayerMove = false;

    // The state of the pawn a movement service belongs to; null for anything that is not a player.
    PlayerState* GetState(CCSPlayer_MovementServices* pServices, CCSPlayerPawn** ppPawn);

    // The player's origin and velocity as the pass sees them: the move data while a
    // pass is in flight, the pawn otherwise.
    static void GetOrigin(const PlayerState& state, CCSPlayerPawn* pPawn, Vector& vecOut);
    static void GetVelocity(const PlayerState& state, CCSPlayerPawn* pPawn, Vector& vecOut);
    static void SetOrigin(PlayerState& state, CCSPlayerPawn* pPawn, const Vector& vecOrigin);
    static void SetVelocity(PlayerState& state, CCSPlayerPawn* pPawn, const Vector& vecVelocity);
    static void GetBounds(CCSPlayer_MovementServices* pServices, PlayerBounds_t& bounds);

    void RegisterLanding(PlayerState& state, CCSPlayerPawn* pPawn, const Vector& vecVelocity);
    void ApplySlopeFix(PlayerState& state, CCSPlayerPawn* pPawn, CCSPlayer_MovementServices* pServices);
    static bool IsValidMovementTrace(const MoveTrace& trace, const PlayerBounds_t& bounds, CTraceFilter* pFilter);

    // The player box through the core's trace (CNavPhysicsInterface::Nav_TraceShape),
    // which is what the engine's gone TracePlayerBBox became.
    static void TracePlayerBBox(const Vector& start, const Vector& end, const PlayerBounds_t& bounds, CTraceFilter* pFilter, MoveTrace& pm);

    PlayerState m_states[64];
};
