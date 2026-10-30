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

#include "rampbug.h"

#include <source2mod/sdk/game/shared/tracefilter.h>
#include <source2mod/sdk/schema/classes/CBasePlayerController.h>
#include <source2mod/sdk/schema/classes/CBasePlayerPawn.h>
#include <source2mod/sdk/schema/classes/CBodyComponent.h>
#include <source2mod/sdk/schema/classes/CCSPlayerController.h>
#include <source2mod/sdk/schema/classes/CCSPlayerPawn.h>
#include <source2mod/sdk/schema/classes/CCSPlayer_MovementServices.h>
#include <source2mod/sdk/schema/classes/CGameSceneNode.h>

#include <const.h>
#include <mathlib/mathlib.h>
#include <mathlib/ray.h>

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr int MAX_BUMPS = 4;
    constexpr float RAMP_PIERCE_DISTANCE = 0.0625f;
    constexpr float RAMP_BUG_THRESHOLD = 0.98f;
    constexpr float RAMP_BUG_VELOCITY_THRESHOLD = 0.95f;
    constexpr float NEW_RAMP_THRESHOLD = 0.95f;

    void ClipVelocity(const Vector& in, const Vector& normal, Vector& out)
    {
        float backoff = -((in.x * normal.x) + ((normal.z * in.z) + (in.y * normal.y)));
        backoff = fmaxf(backoff, 0.0f) + 0.03125f;

        out = normal * backoff + in;
    }
}

void CRampbugFix::PlayerState::Reset()
{
    pMoveData = nullptr;
    bProcessingMovement = false;
    bPreviousOnGround = false;
    nGameCodeMoves = 0;
    ForgetSurface();
}

void CRampbugFix::PlayerState::ForgetSurface()
{
    bDidTPM = false;
    bOverrideTPM = false;
    vecTPMVelocity = vec3_invalid;
    vecTPMOrigin = vec3_invalid;
    vecLastValidPlane = vec3_origin;
    vecTakeoffVelocity = vec3_origin;
    vecLandingOrigin = vec3_origin;
    vecLandingVelocity = vec3_origin;
}

void CRampbugFix::ReadConfig(const FixConfig& config)
{
    m_bOverrideTryPlayerMove = config.GetBool("tryplayermove_override", false);
}

bool CRampbugFix::Load(char* error, size_t maxlen)
{
    if (!g_pSource2ModTrace)
    {
        std::snprintf(error, maxlen, "the core has no trace interface (CNavPhysicsInterface)");
        return false;
    }

    for (PlayerState& state : m_states)
        state.Reset();

    // void CCSPlayer_MovementServices::ProcessMovement(void* pMove): pre and post
    if (!Hook(HookPoint::ProcessMovement, HookTarget::All(), { this, &CRampbugFix::OnProcessMovement }, false)
        || !Hook(HookPoint::ProcessMovement, HookTarget::All(), { this, &CRampbugFix::OnProcessMovement }, true))
    {
        std::snprintf(error, maxlen, "CCSPlayer_MovementServices::ProcessMovement cannot be hooked on this game");
        return false;
    }

    // void CCSPlayer_MovementServices::TryPlayerMove(CMoveData* mv, Vector* pFirstDest, trace_t* pFirstTrace, bool* bIsSurfing): pre and post
    if (!Hook(HookPoint::TryPlayerMove, HookTarget::All(), { this, &CRampbugFix::OnTryPlayerMove }, false)
        || !Hook(HookPoint::TryPlayerMove, HookTarget::All(), { this, &CRampbugFix::OnTryPlayerMove }, true))
    {
        std::snprintf(error, maxlen, "CCSPlayer_MovementServices::TryPlayerMove cannot be hooked on this game");
        return false;
    }

    // void CCSPlayer_MovementServices::CategorizePosition(CMoveData* mv, bool bStayOnGround): pre
    if (!Hook(HookPoint::CategorizePosition, HookTarget::All(), { this, &CRampbugFix::OnCategorizePosition }, false))
    {
        std::snprintf(error, maxlen, "CCSPlayer_MovementServices::CategorizePosition cannot be hooked on this game");
        return false;
    }

    Log("hooked ProcessMovement, TryPlayerMove and CategorizePosition; TryPlayerMove override %s", m_bOverrideTryPlayerMove ? "on" : "off");
    return true;
}

void CRampbugFix::Unload()
{
    // The hooks are taken down by the plugin.
}

void CRampbugFix::TracePlayerBBox(const Vector& start, const Vector& end, const PlayerBounds_t& bounds, CTraceFilter* pFilter, MoveTrace& pm)
{
    Ray_t ray;
    ray.Init(bounds.mins, bounds.maxs);

    const TraceResult result = g_pSource2ModTrace->TraceShapeEx(start, end, pFilter, &ray);

    pm.m_vStartPos = result.StartPos();
    pm.m_vEndPos = result.EndPos();
    pm.m_vHitNormal = result.Normal();
    pm.m_flFraction = result.Fraction();
    pm.m_bStartInSolid = result.IsAllSolid();
}

void CRampbugFix::OnStartupServer(StartupServerContext& ctx)
{
    for (PlayerState& state : m_states)
        state.Reset();
}

CRampbugFix::PlayerState* CRampbugFix::GetState(CCSPlayer_MovementServices* pServices, CCSPlayerPawn** ppPawn)
{
    CCSPlayerPawn* pPawn = pServices ? pServices->GetPlayerPawn() : nullptr;
    if (!pPawn)
        return nullptr;

    CBasePlayerController* pController = pPawn->GetController();
    if (!pController)
        return nullptr;

    const int nSlot = pController->GetSlot();
    if (nSlot < 0 || nSlot >= 64)
        return nullptr;

    if (ppPawn)
        *ppPawn = pPawn;

    return &m_states[nSlot];
}

// Inside a pass the move data is where the player is: the game writes the scene
// node's origin only after each subtick step, so reading that one would trace
// from where the player was before this step's move.
void CRampbugFix::GetOrigin(const PlayerState& state, CCSPlayerPawn* pPawn, Vector& vecOut)
{
    if (state.bProcessingMovement && state.pMoveData)
        vecOut = state.pMoveData->m_vecAbsOrigin;
    else
        vecOut = pPawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
}

void CRampbugFix::GetVelocity(const PlayerState& state, CCSPlayerPawn* pPawn, Vector& vecOut)
{
    if (state.bProcessingMovement && state.pMoveData)
        vecOut = state.pMoveData->m_vecVelocity;
    else
        vecOut = pPawn->m_vecAbsVelocity();
}

void CRampbugFix::SetOrigin(PlayerState& state, CCSPlayerPawn* pPawn, const Vector& vecOrigin)
{
    // Only ever reached from inside a pass, where the move data is what the game reads back.
    if (state.bProcessingMovement && state.pMoveData)
        state.pMoveData->m_vecAbsOrigin = vecOrigin;
    else
        pPawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin() = vecOrigin;
}

void CRampbugFix::SetVelocity(PlayerState& state, CCSPlayerPawn* pPawn, const Vector& vecVelocity)
{
    if (state.bProcessingMovement && state.pMoveData)
        state.pMoveData->m_vecVelocity = vecVelocity;
    else
        pPawn->m_vecAbsVelocity() = vecVelocity;
}

void CRampbugFix::GetBounds(CCSPlayer_MovementServices* pServices, PlayerBounds_t& bounds)
{
    bounds.mins = Vector(-16.0f, -16.0f, 0.0f);
    bounds.maxs = Vector(16.0f, 16.0f, pServices->m_bDucked() ? 54.0f : 72.0f);
}

void CRampbugFix::RegisterLanding(PlayerState& state, CCSPlayerPawn* pPawn, const Vector& vecVelocity)
{
    GetOrigin(state, pPawn, state.vecLandingOrigin);

    state.vecLandingVelocity = vecVelocity;
}

// Landing on a slope: the game clips the landing velocity against the slope
// and loses speed the player should keep.
void CRampbugFix::ApplySlopeFix(PlayerState& state, CCSPlayerPawn* pPawn, CCSPlayer_MovementServices* pServices)
{
    if (!state.bProcessingMovement || !state.pMoveData)
        return;

    PlayerBounds_t bounds;
    GetBounds(pServices, bounds);

    CTraceFilterPlayerMovementCS filter(pPawn);
    Vector ground = state.pMoveData->m_vecAbsOrigin;
    ground.z -= 2.0f;
    MoveTrace trace;

    TracePlayerBBox(state.pMoveData->m_vecAbsOrigin, ground, bounds, &filter, trace);

    if (trace.m_bStartInSolid || trace.m_flFraction == 1.0f || trace.m_vHitNormal.z < 0.7f || trace.m_vHitNormal.z >= 1.0f)
        return;

    Vector newVelocity;
    const float backoff = DotProduct(state.vecLandingVelocity, trace.m_vHitNormal);

    for (int i = 0; i < 3; i++)
    {
        const float change = trace.m_vHitNormal[i] * backoff;
        newVelocity[i] = state.vecLandingVelocity[i] - change;
    }

    const float adjust = DotProduct(newVelocity, trace.m_vHitNormal);
    if (adjust < 0.0f)
        newVelocity -= trace.m_vHitNormal * adjust;

    if (newVelocity.Length2D() >= state.vecLandingVelocity.Length2D())
    {
        state.pMoveData->m_vecVelocity.x = newVelocity.x;
        state.pMoveData->m_vecVelocity.y = newVelocity.y;
        state.vecLandingVelocity.x = newVelocity.x;
        state.vecLandingVelocity.y = newVelocity.y;
    }
}

bool CRampbugFix::IsValidMovementTrace(const MoveTrace& trace, const PlayerBounds_t& bounds, CTraceFilter* pFilter)
{
    if (trace.m_bStartInSolid)
        return false;

    // Hit something, but no plane to show for it.
    if (trace.m_flFraction < 1.0f && fabsf(trace.m_vHitNormal.x) < FLT_EPSILON && fabsf(trace.m_vHitNormal.y) < FLT_EPSILON && fabsf(trace.m_vHitNormal.z) < FLT_EPSILON)
        return false;

    // A deformed plane.
    if (fabsf(trace.m_vHitNormal.x) > 1.0f || fabsf(trace.m_vHitNormal.y) > 1.0f || fabsf(trace.m_vHitNormal.z) > 1.0f)
        return false;

    // An unswept trace and a backward one, to be sure.
    MoveTrace stuck;
    TracePlayerBBox(trace.m_vEndPos, trace.m_vEndPos, bounds, pFilter, stuck);
    if (stuck.m_bStartInSolid || stuck.m_flFraction < 1.0f - FLT_EPSILON)
        return false;

    // Since the Call to Arms update a trace can hit in one direction only, so
    // the backward fraction is not checked.
    TracePlayerBBox(trace.m_vEndPos, trace.m_vStartPos, bounds, pFilter, stuck);
    if (stuck.m_bStartInSolid)
        return false;

    return true;
}

HookReturn<bool> CRampbugFix::OnProcessMovement(MovementContext& ctx, bool post)
{
    if (post)
    {
        GF_VPROF("GameFixes::rampbug::ProcessMovementPost");
        ProcessMovementPost(ctx.services);
    }
    else
    {
        GF_VPROF("GameFixes::rampbug::ProcessMovement");
        ProcessMovementPre(ctx.services, ctx.moveData);
    }

    return Action::Ignore;
}

void CRampbugFix::ProcessMovementPre(CCSPlayer_MovementServices* pThis, CMoveData* pMove)
{
    CCSPlayerPawn* pPawn = nullptr;
    PlayerState* pState = GetState(pThis, &pPawn);
    if (!pState)
        return;

    // Moved by game code since the last pass (a teleport): the surface it was on is not where it is now
    const int nGameCodeMoves = pThis->m_nGameCodeHasMovedPlayerAfterCommand();
    if (nGameCodeMoves != pState->nGameCodeMoves)
    {
        pState->nGameCodeMoves = nGameCodeMoves;
        pState->ForgetSurface();
        pState->bPreviousOnGround = (pPawn->m_fFlags() & FL_ONGROUND) != 0;
    }

    pState->pMoveData = pMove;
    pState->bDidTPM = false;
    pState->bProcessingMovement = true;

    const bool bOnGround = (pPawn->m_fFlags() & FL_ONGROUND) != 0;
    if (!pState->bPreviousOnGround && bOnGround)
    {
        Vector velocity;
        GetVelocity(*pState, pPawn, velocity);
        RegisterLanding(*pState, pPawn, velocity);
        ApplySlopeFix(*pState, pPawn, pThis);
    }
    else if (pState->bPreviousOnGround && !bOnGround)
    {
        GetVelocity(*pState, pPawn, pState->vecTakeoffVelocity);
    }
}

void CRampbugFix::ProcessMovementPost(CCSPlayer_MovementServices* pThis)
{
    CCSPlayerPawn* pPawn = nullptr;
    PlayerState* pState = GetState(pThis, &pPawn);
    if (!pState)
        return;

    if (!pState->bDidTPM)
        pState->vecLastValidPlane = vec3_origin;

    pState->bProcessingMovement = false;
    pState->pMoveData = nullptr;
    pState->bPreviousOnGround = (pPawn->m_fFlags() & FL_ONGROUND) != 0;
}

HookReturn<void> CRampbugFix::OnTryPlayerMove(TryPlayerMoveContext& ctx, bool post)
{
    if (post)
    {
        GF_VPROF("GameFixes::rampbug::TryPlayerMovePost");
        TryPlayerMovePost(ctx);
    }
    else
    {
        GF_VPROF("GameFixes::rampbug::TryPlayerMove");
        TryPlayerMovePre(ctx);
    }

    return Action::Ignore;
}

// The pass's own walk along the ramp, redone with every trace nudged off the
// last valid plane, so a trace that stops a hair short of the ramp (or on
// its edge) cannot hand the game a plane the player never hit.
void CRampbugFix::TryPlayerMovePre(TryPlayerMoveContext& ctx)
{
    CCSPlayer_MovementServices* pThis = ctx.services;
    Vector* pFirstDest = ctx.firstDest;
    const CGameTrace* pFirstTrace = ctx.firstTrace;

    CCSPlayerPawn* pPawn = nullptr;
    PlayerState* pState = GetState(pThis, &pPawn);
    if (!pState)
        return;

    PlayerState& state = *pState;
    state.bOverrideTPM = false;
    state.bDidTPM = true;

    const CGlobalVars* pGlobals = g_pSource2ModAPI->GetGlobalVars();
    if (!pGlobals)
        return;

    float timeLeft = pGlobals->frametime;

    Vector start, velocity, end;
    GetOrigin(state, pPawn, start);
    GetVelocity(state, pPawn, velocity);

    if (velocity.Length() == 0.0f)
        return;

    const Vector primalVelocity = velocity;
    bool validPlane = false;

    float allFraction = 0.0f;
    MoveTrace pm;
    unsigned bumpCount = 0;
    Vector planes[5];
    unsigned numPlanes = 0;
    MoveTrace pierce;

    PlayerBounds_t bounds;
    GetBounds(pThis, bounds);

    CTraceFilterPlayerMovementCS filter(pPawn);

    bool potentiallyStuck = false;

    for (bumpCount = 0; bumpCount < MAX_BUMPS; bumpCount++)
    {
        // Assume the whole way from the origin to the end point is free.
        VectorMA(start, timeLeft, velocity, end);

        if (pFirstDest && pFirstTrace && end == *pFirstDest)
        {
            pm.m_vStartPos = pFirstTrace->m_vStartPos;
            pm.m_vEndPos = pFirstTrace->m_vEndPos;
            pm.m_vHitNormal = pFirstTrace->m_vHitNormal;
            pm.m_flFraction = pFirstTrace->m_flFraction;
            pm.m_bStartInSolid = pFirstTrace->m_bStartInSolid;
        }
        else
        {
            TracePlayerBBox(start, end, bounds, &filter, pm);
            if (end == start)
                continue;

            if (IsValidMovementTrace(pm, bounds, &filter) && pm.m_flFraction == 1.0f)
                break;  // nothing in the way

            if (state.vecLastValidPlane.Length() > FLT_EPSILON
                && (!IsValidMovementTrace(pm, bounds, &filter) || pm.m_vHitNormal.Dot(state.vecLastValidPlane) < RAMP_BUG_THRESHOLD
                    || (potentiallyStuck && pm.m_flFraction == 0.0f)))
            {
                // A plane that would change the velocity a lot: make sure it is a real one.
                Vector offsetDirection;
                const float offsets[] = {0.0f, -1.0f, 1.0f};
                bool success = false;
                for (unsigned i = 0; i < 3 && !success; i++)
                {
                    for (unsigned j = 0; j < 3 && !success; j++)
                    {
                        for (unsigned k = 0; k < 3 && !success; k++)
                        {
                            if (i == 0 && j == 0 && k == 0)
                            {
                                offsetDirection = state.vecLastValidPlane;
                            }
                            else
                            {
                                offsetDirection = Vector(offsets[i], offsets[j], offsets[k]);
                                if (state.vecLastValidPlane.Dot(offsetDirection) <= 0.0f)
                                    continue;

                                MoveTrace test;
                                TracePlayerBBox(start + offsetDirection * RAMP_PIERCE_DISTANCE, start, bounds, &filter, test);
                                if (!IsValidMovementTrace(test, bounds, &filter))
                                    continue;
                            }

                            bool goodTrace = false;
                            float ratio = 0.0f;
                            bool hitNewPlane = false;
                            for (ratio = 0.25f; ratio <= 1.0f; ratio += 0.25f)
                            {
                                TracePlayerBBox(start + offsetDirection * RAMP_PIERCE_DISTANCE * ratio, end + offsetDirection * RAMP_PIERCE_DISTANCE * ratio, bounds, &filter, pierce);
                                if (!IsValidMovementTrace(pierce, bounds, &filter))
                                    continue;

                                // Until a similar plane is hit.
                                validPlane = pierce.m_flFraction < 1.0f && pierce.m_flFraction > 0.1f && pierce.m_vHitNormal.Dot(state.vecLastValidPlane) >= RAMP_BUG_THRESHOLD;
                                hitNewPlane = pm.m_vHitNormal.Dot(pierce.m_vHitNormal) < NEW_RAMP_THRESHOLD && state.vecLastValidPlane.Dot(pierce.m_vHitNormal) > NEW_RAMP_THRESHOLD;
                                goodTrace = CloseEnough(pierce.m_flFraction, 1.0f, FLT_EPSILON) || validPlane;
                                if (goodTrace)
                                    break;
                            }

                            if (goodTrace || hitNewPlane)
                            {
                                // Back to the original end point, for its normal.
                                MoveTrace test;
                                TracePlayerBBox(pierce.m_vEndPos, end, bounds, &filter, test);
                                pm = pierce;
                                pm.m_vStartPos = start;
                                pm.m_flFraction = clamp((pierce.m_vEndPos - pierce.m_vStartPos).Length() / (end - start).Length(), 0.0f, 1.0f);
                                pm.m_vEndPos = test.m_vEndPos;
                                if (pierce.m_vHitNormal.Length() > 0.0f)
                                {
                                    pm.m_vHitNormal = pierce.m_vHitNormal;
                                    state.vecLastValidPlane = pierce.m_vHitNormal;
                                }
                                else
                                {
                                    pm.m_vHitNormal = test.m_vHitNormal;
                                    state.vecLastValidPlane = test.m_vHitNormal;
                                }
                                success = true;
                                state.bOverrideTPM = true;
                            }
                        }
                    }
                }
            }

            if (pm.m_vHitNormal.Length() > 0.99f)
                state.vecLastValidPlane = pm.m_vHitNormal;

            potentiallyStuck = pm.m_flFraction == 0.0f;
        }

        if (pm.m_flFraction * velocity.Length() > 0.03125f || pm.m_flFraction > 0.03125f)
        {
            allFraction += pm.m_flFraction;
            start = pm.m_vEndPos;
            numPlanes = 0;
        }

        if (allFraction == 1.0f)
            break;

        timeLeft -= pGlobals->frametime * pm.m_flFraction;

        if (numPlanes >= 5 || (pm.m_vHitNormal.z >= 0.7f && velocity.Length2D() < 1.0f))
        {
            velocity = vec3_origin;
            break;
        }

        planes[numPlanes] = pm.m_vHitNormal;
        numPlanes++;

        if (numPlanes == 1 && pPawn->m_MoveType() == MOVETYPE_WALK && pPawn->m_hGroundEntity().Get() == nullptr)
        {
            ClipVelocity(velocity, planes[0], velocity);
        }
        else
        {
            unsigned i, j;
            for (i = 0; i < numPlanes; i++)
            {
                ClipVelocity(velocity, planes[i], velocity);
                for (j = 0; j < numPlanes; j++)
                {
                    if (j != i && velocity.Dot(planes[j]) < 0)
                        break;  // moving against this plane now
                }

                if (j == numPlanes)
                    break;  // no clip needed
            }

            if (i == numPlanes)
            {
                // Along the crease.
                if (numPlanes != 2)
                {
                    velocity = vec3_origin;
                    break;
                }

                Vector dir;
                CrossProduct(planes[0], planes[1], dir);
                dir.NormalizeInPlace();
                const float d = dir.Dot(velocity);
                VectorScale(dir, d, velocity);

                if (velocity.Dot(primalVelocity) <= 0)
                {
                    velocity = vec3_origin;
                    break;
                }
            }
        }
    }

    state.vecTPMOrigin = pm.m_vEndPos;
    state.vecTPMVelocity = velocity;
}

void CRampbugFix::TryPlayerMovePost(TryPlayerMoveContext& ctx)
{
    CCSPlayerPawn* pPawn = nullptr;
    PlayerState* pState = GetState(ctx.services, &pPawn);
    if (!pState)
        return;

    // Our walk ending in a standstill is no result to put back
    if (!m_bOverrideTryPlayerMove || !pState->bOverrideTPM || pState->vecTPMVelocity == vec3_invalid || pState->vecTPMVelocity.LengthSqr() < 1.0f)
        return;

    Vector velocity;
    GetVelocity(*pState, pPawn, velocity);

    // The game took the player somewhere our own walk did not: put ours back.
    const bool velocityHeavilyModified = pState->vecTPMVelocity.Normalized().Dot(velocity.Normalized()) < RAMP_BUG_THRESHOLD
        || (pState->vecTPMVelocity.Length() > 50.0f && velocity.Length() / pState->vecTPMVelocity.Length() < RAMP_BUG_VELOCITY_THRESHOLD);

    if (pState->bOverrideTPM && velocityHeavilyModified && pState->vecTPMOrigin != vec3_invalid && pState->vecTPMVelocity != vec3_invalid)
    {
        SetOrigin(*pState, pPawn, pState->vecTPMOrigin);
        SetVelocity(*pState, pPawn, pState->vecTPMVelocity);
    }
}

// Going down a ramp fast, the ground check can find a standable plane the
// player is not really on; nudge them onto the last valid plane first.
HookReturn<void> CRampbugFix::OnCategorizePosition(CategorizePositionContext& ctx, bool post)
{
    GF_VPROF("GameFixes::rampbug::CategorizePosition");

    CCSPlayerPawn* pPawn = nullptr;
    PlayerState* pState = GetState(ctx.services, &pPawn);
    if (!pState)
        return Action::Ignore;

    PlayerState& state = *pState;

    // Already colliding with a standable, valid plane: nothing to check.
    if (ctx.stayOnGround || state.vecLastValidPlane.Length() < 0.000001f || state.vecLastValidPlane.z > 0.7f)
        return Action::Ignore;

    Vector velocity;
    GetVelocity(state, pPawn, velocity);

    // Only worth it going down at some speed.
    if (velocity.z > -64.0f)
        return Action::Ignore;

    PlayerBounds_t bounds;
    GetBounds(ctx.services, bounds);

    CTraceFilterPlayerMovementCS filter(pPawn);
    MoveTrace trace;

    Vector origin, groundOrigin;
    GetOrigin(state, pPawn, origin);
    groundOrigin = origin;
    groundOrigin.z -= 2.0f;

    TracePlayerBBox(origin, groundOrigin, bounds, &filter, trace);

    if (trace.m_flFraction == 1.0f)
        return Action::Ignore;

    // Something the player could actually stand on?
    if (trace.m_flFraction < 0.95f && trace.m_vHitNormal.z > 0.7f && state.vecLastValidPlane.Dot(trace.m_vHitNormal) < RAMP_BUG_THRESHOLD)
    {
        origin += state.vecLastValidPlane * 0.0625f;
        groundOrigin = origin;
        groundOrigin.z -= 2.0f;
        TracePlayerBBox(origin, groundOrigin, bounds, &filter, trace);

        if (trace.m_bStartInSolid)
            return Action::Ignore;

        if (trace.m_flFraction == 1.0f || state.vecLastValidPlane.Dot(trace.m_vHitNormal) >= RAMP_BUG_THRESHOLD)
            SetOrigin(state, pPawn, origin);
    }

    return Action::Ignore;
}
