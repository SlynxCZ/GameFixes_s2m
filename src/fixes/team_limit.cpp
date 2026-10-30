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

#include "team_limit.h"

#include <source2mod/sdk/schema/classes/CCSGameRules.h>

void CTeamLimitFix::OnGameFrame(bool simulating)
{
    if (!simulating)
        return;

    GF_VPROF("GameFixes::team_limit::OnGameFrame");

    // The core keeps the active rules object (null before cs_gamerules exists,
    // and across a map change until the new one is created).
    CCSGameRules* pGameRules = g_pSource2ModEntities->GetGameRules();
    CGlobalVars* pGlobals = g_pEngineServer->GetServerGlobals();
    if (!pGameRules || !pGlobals)
        return;

    // None of these are networked; a plain write is enough
    const int iMaxPlayers = pGlobals->maxClients;
    for (int32_t* pLimit : { &pGameRules->m_iMaxNumTerrorists.Get(), &pGameRules->m_iMaxNumCTs.Get(),
                             &pGameRules->m_iSpawnPointCount_Terrorist.Get(), &pGameRules->m_iSpawnPointCount_CT.Get() })
    {
        if (*pLimit < iMaxPlayers)
            *pLimit = iMaxPlayers;
    }
}
