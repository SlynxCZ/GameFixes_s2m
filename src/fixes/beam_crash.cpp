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
#include "beam_crash.h"

#include <source2mod/sdk/schema/classes/CBeam.h>
#include <source2mod/sdk/schema/classes/CBodyComponent.h>
#include <source2mod/sdk/schema/classes/CGameSceneNode.h>

namespace
{
    // The scene node of a beam, nullptr when it has none (or the schema lacks the fields).
    CGameSceneNode* SceneNodeOf(CBeam* pBeam)
    {
        if (!pBeam)
            return nullptr;

        CBodyComponent* pBody = pBeam->m_CBodyComponent.Get();
        return pBody ? pBody->m_pSceneNode.Get() : nullptr;
    }

    // What the game's function leaves in the beam: the node's absolute origin / the end position.
    // Plain writes, as the game's own code does it -- no network state change.
    void StoreOrigin(CBeam* pBeam, const Vector* pVecPosition)
    {
        if (CGameSceneNode* pNode = SceneNodeOf(pBeam))
            pNode->m_vecAbsOrigin.Get() = *pVecPosition;
    }

    void StoreEndPos(CBeam* pBeam, const Vector* pVecPosition)
    {
        pBeam->m_vecEndPos.Get() = *pVecPosition;
    }

    // Game code still works for parented beams; without a parent it would loop forever, so it is skipped
    // (the caller stores the result itself).
    bool HasParent(CBeam* pBeam)
    {
        CGameSceneNode* pNode = SceneNodeOf(pBeam);
        return pNode && pNode->m_pParent.Get();
    }
}

bool CBeamCrashFix::Load(char* error, size_t maxlen)
{
    // https://github.com/Source2ZE/CS2Fixes/commit/0686a807d790ef22407aa83e33b40bbed6531b51

    // void CBeam::SetBeamOrigin(const Vector* pVecPosition)
    ISource2ModFunction* pSetBeamOrigin = FindFunction("CBeam::SetBeamOrigin", error, maxlen);
    if (!pSetBeamOrigin)
        return false;

    // void CBeam::SetBeamEndPos(const Vector* pVecPosition)
    ISource2ModFunction* pSetBeamEndPos = FindFunction("CBeam::SetBeamEndPos", error, maxlen);
    if (!pSetBeamEndPos)
        return false;

    // Pre: parented beams run the game's code, the rest is skipped and done here. The game's function does not
    // run for those, so the result is stored in the pre handler as well (the post handler stores it again, as
    // mm's did -- also after the game's code for a parented beam).
    const bool bHooked =
        Hook(HookPoint::Function(pSetBeamOrigin), HookTarget::All(),
             [](FunctionContext& ctx, bool post) -> HookReturn<void>
             {
                 GF_VPROF("GameFixes::beam_crash::SetBeamOrigin");

                 CBeam* pBeam = ctx.GetArg<CBeam*>(0);
                 const Vector* pVecPosition = ctx.GetArg<const Vector*>(1);
                 if (!pBeam || !pVecPosition || HasParent(pBeam))
                     return Action::Ignore;

                 StoreOrigin(pBeam, pVecPosition);
                 return Action::Supersede;
             }) &&
        Hook(HookPoint::Function(pSetBeamOrigin), HookTarget::All(),
             [](FunctionContext& ctx, bool post) -> HookReturn<void>
             {
                 GF_VPROF("GameFixes::beam_crash::SetBeamOriginPost");

                 CBeam* pBeam = ctx.GetArg<CBeam*>(0);
                 const Vector* pVecPosition = ctx.GetArg<const Vector*>(1);
                 if (pBeam && pVecPosition)
                     StoreOrigin(pBeam, pVecPosition);
                 return Action::Ignore;
             }, true) &&
        Hook(HookPoint::Function(pSetBeamEndPos), HookTarget::All(),
             [](FunctionContext& ctx, bool post) -> HookReturn<void>
             {
                 GF_VPROF("GameFixes::beam_crash::SetBeamEndPos");

                 CBeam* pBeam = ctx.GetArg<CBeam*>(0);
                 const Vector* pVecPosition = ctx.GetArg<const Vector*>(1);
                 if (!pBeam || !pVecPosition || HasParent(pBeam))
                     return Action::Ignore;

                 StoreEndPos(pBeam, pVecPosition);
                 return Action::Supersede;
             }) &&
        Hook(HookPoint::Function(pSetBeamEndPos), HookTarget::All(),
             [](FunctionContext& ctx, bool post) -> HookReturn<void>
             {
                 GF_VPROF("GameFixes::beam_crash::SetBeamEndPosPost");

                 CBeam* pBeam = ctx.GetArg<CBeam*>(0);
                 const Vector* pVecPosition = ctx.GetArg<const Vector*>(1);
                 if (pBeam && pVecPosition)
                     StoreEndPos(pBeam, pVecPosition);
                 return Action::Ignore;
             }, true);

    if (!bHooked)
    {
        snprintf(error, maxlen, "could not hook CBeam::SetBeamOrigin / SetBeamEndPos");
        return false;
    }

    Log("hooked CBeam::SetBeamOrigin (%p) and CBeam::SetBeamEndPos (%p)", pSetBeamOrigin->GetAddress(), pSetBeamEndPos->GetAddress());
    return true;
}
