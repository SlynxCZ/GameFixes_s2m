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

#include <algorithm>
#include <cstdlib>
#include <cstring>

bool CSvCheatsFix::Load(char* error, size_t maxlen)
{
    m_svCheatsIndex = g_pSource2ModConVars->GetConvarAccessIndexByName("sv_cheats");
    if (m_svCheatsIndex == SOURCE2MOD_INVALID_CONVAR_INDEX)
    {
        snprintf(error, maxlen, "sv_cheats not found");
        return false;
    }

    // The hooks are the fix's own, the plugin / Unload() take them down. They
    // only read and write entity memory, no convar is set from inside them.
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

    // Post: the core runs post listeners only when no pre listener superseded
    // the command, i.e. only when the engine's own noclip is about to run. A
    // plugin answering "noclip" itself (FUNPLAY Core) never reaches this.
    m_noclipHookId = g_pSource2ModCommands->HookConCommand(
        "noclip",
        [this](const Source2ModCommandContext& ctx, const Source2ModCommandArgs& args, bool post) -> Action
        {
            OnNoclipCommand(ctx.GetPlayerSlot());
            return Action::Ignore;
        },
        true);

    if (!m_noclipHookId)
    {
        snprintf(error, maxlen, "the core refused the noclip command listener");
        return false;
    }

    Log("watching sv_cheats");
    return true;
}

void CSvCheatsFix::Unload()
{
    if (m_noclipHookId)
        g_pSource2ModCommands->UnhookConCommand(m_noclipHookId);

    m_noclipHookId = 0;
    m_pending.clear();
    m_tracked.clear();
}

bool CSvCheatsFix::CheatsOn() const
{
    return g_pSource2ModConVars->GetBool(m_svCheatsIndex);
}

void CSvCheatsFix::OnNoclipCommand(CPlayerSlot slot)
{
    // Without sv_cheats the engine refuses the command; from the server
    // console there is no player to put in noclip.
    if (!slot.IsValid() || !CheatsOn())
        return;

    CGameEntitySystem* pEntitySystem = GameEntitySystem();
    if (!pEntitySystem)
        return;

    auto* pController = static_cast<CBasePlayerController*>(pEntitySystem->GetEntityInstance(CEntityIndex(slot.Get() + 1)));
    if (!pController || !pController->IsConnected())
        return;

    CBaseEntity* pPawn = pController->GetPawn();
    if (!pPawn)
        return;

    // The engine runs the command right after this listener; what it did is
    // looked at on the next frame.
    m_pending.push_back({ pPawn->GetHandle(), pPawn->m_MoveType() });
}

void CSvCheatsFix::OnGameFrame(bool simulating)
{
    if (m_pending.empty() && m_tracked.empty())
        return;

    GF_VPROF("GameFixes::sv_cheats::OnGameFrame");

    // Forget pawns that are gone or no longer in noclip: whatever puts them
    // in noclip after that (a plugin's /noclip, /fly, ...) is not ours.
    std::erase_if(m_tracked, [](const CheatNoclip& t)
    {
        CBaseEntity* pPawn = t.hPawn.Get();
        return !pPawn || pPawn->m_MoveType() != MOVETYPE_NOCLIP;
    });

    for (const PendingNoclip& p : m_pending)
    {
        CBaseEntity* pPawn = p.hPawn.Get();
        if (!pPawn)
            continue;

        // A toggle off is dropped by the erase above already.
        if (p.nMoveTypeBefore == MOVETYPE_NOCLIP || pPawn->m_MoveType() != MOVETYPE_NOCLIP)
            continue;

        const bool bKnown = std::any_of(m_tracked.begin(), m_tracked.end(), [&](const CheatNoclip& t) { return t.hPawn == p.hPawn; });
        if (!bKnown)
            m_tracked.push_back({ p.hPawn, pPawn->m_nActualMoveType() });
    }
    m_pending.clear();
}

void CSvCheatsFix::OnSvCheatsDisabled()
{
    // A command noted this frame never ran with cheats on as far as we care.
    m_pending.clear();

    int iFixed = 0;
    for (const CheatNoclip& t : m_tracked)
    {
        CBaseEntity* pPawn = t.hPawn.Get();

        // Still in the very noclip the command gave it: a plugin that took the
        // pawn over in the same frame (another actual move type) keeps it.
        if (!pPawn || pPawn->m_MoveType() != MOVETYPE_NOCLIP || pPawn->m_nActualMoveType() != t.nActualMoveType)
            continue;

        // Both move types together (m_MoveType networked, m_nActualMoveType is
        // what the server's movement runs on), and no leftover noclip speed.
        pPawn->SetMoveType(MOVETYPE_WALK);
        const Vector vecZero(0.0f, 0.0f, 0.0f);
        pPawn->Teleport(nullptr, nullptr, &vecZero);
        iFixed++;
    }
    m_tracked.clear();

    if (iFixed)
        Log("sv_cheats went off, %d pawn(s) taken out of the cheat noclip", iFixed);
}
