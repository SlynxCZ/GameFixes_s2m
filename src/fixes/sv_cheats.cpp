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

#include "sv_cheats.h"

#include <source2mod/sdk/schema/classes/CBasePlayerController.h>
#include <source2mod/sdk/schema/classes/CCSPlayerPawn.h>

#include <gameentitysystem.h>

#include <cstdlib>
#include <cstring>

bool CSvCheatsFix::Load(char* error, size_t maxlen)
{
    // The hook is the fix's own, the plugin takes it down on unload. It only
    // reads and writes entity memory, no convar is set from inside it.
    const Source2ModConVarHookId id = HookConVarChange(
        [this](ConVarRefAbstract* pConVar, CSplitScreenSlot slot, const char* pszNewValue, const char* pszOldValue)
        {
            GF_VPROF("GameFixes::sv_cheats::OnConVarChanged");

            if (!pConVar || std::strcmp(pConVar->GetName(), "sv_cheats") != 0)
                return;

            if (pszNewValue && !std::atoi(pszNewValue))
                OnSvCheatsDisabled();
        });

    if (!id)
    {
        snprintf(error, maxlen, "the core refused the ConVar change hook");
        return false;
    }

    Log("watching sv_cheats");
    return true;
}

void CSvCheatsFix::OnSvCheatsDisabled()
{
    CGameEntitySystem* pEntitySystem = GameEntitySystem();
    CGlobalVars* pGlobals = g_pEngineServer->GetServerGlobals();
    if (!pEntitySystem || !pGlobals)
        return;

    int iFixed = 0;
    for (int i = 0; i < pGlobals->maxClients; i++)
    {
        auto* pController = static_cast<CBasePlayerController*>(pEntitySystem->GetEntityInstance(CEntityIndex(i + 1)));
        if (!pController || !pController->IsConnected())
            continue;

        CBaseEntity* pPawn = pController->GetPawn();
        if (!pPawn || (pPawn->m_MoveType() != MOVETYPE_NOCLIP && pPawn->m_nActualMoveType() != MOVETYPE_NOCLIP))
            continue;

        pPawn->m_MoveType() = MOVETYPE_WALK;
        pPawn->m_nActualMoveType() = MOVETYPE_WALK;
        pPawn->m_MoveType.NetworkStateChanged();
        iFixed++;
    }

    if (iFixed)
        Log("sv_cheats went off, %d pawn(s) taken out of noclip", iFixed);
}
