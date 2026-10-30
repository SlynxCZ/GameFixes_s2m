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

#include "cstv_bot_quota.h"

#include <source2mod/sdk/schema/classes/CBasePlayerController.h>

#include <cstdio>

bool CCstvBotQuotaFix::Load(char* error, size_t maxlen)
{
    ISource2ModFunction* pSelect = FindFunction("BotQuota::SelectAndKickBot", error, maxlen);
    if (!pSelect)
        return false;

    ISource2ModFunction* pCandidate = FindFunction("BotQuota::TeamCandidate", error, maxlen);
    if (!pCandidate)
        return false;

    // The predicate first: it does nothing outside the selection scope, so
    // there is no moment in which it refuses the HLTV for anyone else.
    if (!Hook(HookPoint::Function(pCandidate), HookTarget::All(), { this, &CCstvBotQuotaFix::OnTeamCandidate }))
    {
        std::snprintf(error, maxlen, "could not hook the team predicate");
        return false;
    }

    if (!Hook(HookPoint::Function(pSelect), HookTarget::All(), { this, &CCstvBotQuotaFix::OnSelectAndKickBot }, false) ||
        !Hook(HookPoint::Function(pSelect), HookTarget::All(), { this, &CCstvBotQuotaFix::OnSelectAndKickBot }, true))
    {
        std::snprintf(error, maxlen, "could not hook the bot selection");
        return false;
    }

    Log("keeping the SourceTV client out of bot_quota's bot removal");
    return true;
}

void CCstvBotQuotaFix::Unload()
{
    // The hooks are taken down by the plugin.
    m_nSelectionDepth = 0;
    m_bLoggedSkip = false;
}

void CCstvBotQuotaFix::OnStartupServer(StartupServerContext& ctx)
{
    // A selection never spans a map change; this only guards a Post that did not come.
    m_nSelectionDepth = 0;
    m_bLoggedSkip = false;
}

HookReturn<void> CCstvBotQuotaFix::OnSelectAndKickBot(FunctionContext& ctx, bool post)
{
    if (post)
    {
        if (m_nSelectionDepth > 0)
            m_nSelectionDepth--;
    }
    else
    {
        m_nSelectionDepth++;
    }

    return Action::Ignore;
}

HookReturn<void> CCstvBotQuotaFix::OnTeamCandidate(FunctionContext& ctx, bool post)
{
    if (m_nSelectionDepth <= 0)
        return Action::Ignore;

    GF_VPROF("GameFixes::cstv_bot_quota::TeamCandidate");

    // A pawn-less HLTV client can pass for a bot here: it is not a candidate,
    // so the selection goes on looking for an ordinary bot.
    auto* pController = ctx.GetArg<CBasePlayerController*>(1);
    if (!pController || !pController->m_bIsHLTV())
        return Action::Ignore;

    if (!m_bLoggedSkip)
    {
        m_bLoggedSkip = true;
        Log("skipped the SourceTV client during bot removal (first time this map)");
    }

    ctx.SetReturn<bool>(false);
    return Action::Supersede;
}
