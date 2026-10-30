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

#include "water_jump.h"

#include <source2mod/sdk/schema/classes/CBasePlayerPawn.h>
#include <source2mod/sdk/schema/classes/CCSPlayerController.h>
#include <source2mod/sdk/schema/classes/CCSPlayerPawn.h>
#include <source2mod/sdk/schema/classes/CCSPlayer_MovementServices.h>

#include <cstdio>
#include <cstring>

bool CWaterJumpFix::Load(char* error, size_t maxlen)
{
    std::memset(m_bSkipNextCategorize, 0, sizeof(m_bSkipNextCategorize));

    // CCSPlayer_MovementServices::WaterMove(CMoveData* mv): pre, the original still runs.
    if (!Hook(HookPoint::WaterMove, HookTarget::All(), { this, &CWaterJumpFix::OnWaterMove }))
    {
        std::snprintf(error, maxlen, "CCSPlayer_MovementServices::WaterMove cannot be hooked on this game");
        return false;
    }

    // CCSPlayer_MovementServices::CategorizePosition(CMoveData* mv, bool bStayOnGround): pre.
    if (!Hook(HookPoint::CategorizePosition, HookTarget::All(), { this, &CWaterJumpFix::OnCategorizePosition }))
    {
        std::snprintf(error, maxlen, "CCSPlayer_MovementServices::CategorizePosition cannot be hooked on this game");
        return false;
    }

    Log("hooked WaterMove and CategorizePosition");
    return true;
}

void CWaterJumpFix::Unload()
{
    // The hooks are taken down by the plugin.
}

void CWaterJumpFix::OnStartupServer(StartupServerContext& ctx)
{
    std::memset(m_bSkipNextCategorize, 0, sizeof(m_bSkipNextCategorize));
}

int CWaterJumpFix::GetSlot(CCSPlayer_MovementServices* pServices)
{
    CCSPlayerPawn* pPawn = pServices ? pServices->GetPlayerPawn() : nullptr;
    CBasePlayerController* pController = pPawn ? pPawn->GetController() : nullptr;
    if (!pController)
        return -1;

    const int nSlot = pController->GetSlot();
    return (nSlot >= 0 && nSlot < 64) ? nSlot : -1;
}

HookReturn<bool> CWaterJumpFix::OnWaterMove(MovementContext& ctx, bool post)
{
    GF_VPROF("GameFixes::water_jump::WaterMove");

    const int nSlot = GetSlot(ctx.services);
    if (nSlot >= 0)
        m_bSkipNextCategorize[nSlot] = true;

    return Action::Ignore;
}

HookReturn<void> CWaterJumpFix::OnCategorizePosition(CategorizePositionContext& ctx, bool post)
{
    GF_VPROF("GameFixes::water_jump::CategorizePosition");

    const int nSlot = GetSlot(ctx.services);
    if (nSlot < 0 || !m_bSkipNextCategorize[nSlot])
        return Action::Ignore;

    m_bSkipNextCategorize[nSlot] = false;
    return Action::Supersede;
}
