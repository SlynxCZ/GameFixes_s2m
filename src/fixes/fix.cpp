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
#include "fix.h"

#include <cstdarg>
#include <cstdio>

void CFix::Log(const char* pszFormat, ...) const
{
    char szMessage[1024];

    va_list args;
    va_start(args, pszFormat);
    std::vsnprintf(szMessage, sizeof(szMessage), pszFormat, args);
    va_end(args);

    GF_Log("%s: %s", GetName(), szMessage);
}

Source2ModEventHookId CFix::HookEvent(const char* pszEvent, GameEventHandler handler, bool post)
{
    const Source2ModEventHookId id = g_pSource2ModEvents->HookGameEvent(pszEvent, std::move(handler), post);
    if (id)
        m_eventHookIds.push_back(id);
    return id;
}

Source2ModConVarHookId CFix::HookConVarChange(ConVarChangeHandler handler)
{
    const Source2ModConVarHookId id = g_pSource2ModConVars->HookConVarChange(std::move(handler));
    if (id)
        m_conVarHookIds.push_back(id);
    return id;
}

Timer* CFix::AddRepeatingTimer(float flInterval, TimerCallback callback)
{
    Timer* pTimer = g_pSource2ModScheduler->AddTimer(flInterval, std::move(callback), TIMER_FLAG_REPEAT);
    if (pTimer)
        m_timers.push_back(pTimer);
    return pTimer;
}

ISource2ModFunction* CFix::FindFunction(const char* pszName, char* error, size_t maxlen) const
{
    ISource2ModFunction* pFunction = g_pSource2ModFunctions->Find(pszName);
    if (!pFunction && error && maxlen)
    {
        std::snprintf(error, maxlen, "gamedata function \"%s\" not found, or its signature does not match this game "
                                     "(game_fixes.json / gamedata.json; the reason is on the console)", pszName);
    }
    return pFunction;
}

bool CFix::FindOffset(const char* pszName, int& offset, char* error, size_t maxlen) const
{
    offset = g_pSource2ModGameConfig->GetOffset(pszName);
    if (offset >= 0)
        return true;

    if (error && maxlen)
        std::snprintf(error, maxlen, "gamedata offset \"%s\" not found for this platform (game_fixes.json / gamedata.json)", pszName);
    return false;
}

ISource2ModMemory CFix::FindAddress(const char* pszName, char* error, size_t maxlen) const
{
    ISource2ModMemory address = g_pSource2ModGameConfig->ResolveSignature(pszName);
    if (!address && error && maxlen)
        std::snprintf(error, maxlen, "gamedata \"%s\" did not resolve: no entry for this platform, or the module / pattern "
                                     "was not found (game_fixes.json / gamedata.json)", pszName);
    return address;
}

void CFix::ReleaseResources()
{
    // The hooks, events and timers are the plugin's until it unloads, so they
    // are still alive here. A timer that is running right now is only marked.
    for (Timer* pTimer : m_timers)
        g_pSource2ModScheduler->KillTimer(pTimer);
    m_timers.clear();

    for (const Source2ModConVarHookId id : m_conVarHookIds)
        g_pSource2ModConVars->UnhookConVarChange(id);
    m_conVarHookIds.clear();

    for (const Source2ModEventHookId id : m_eventHookIds)
        g_pSource2ModEvents->UnhookGameEvent(id);
    m_eventHookIds.clear();

    for (const Source2ModHookId id : m_hookIds)
        g_pSource2ModHooks->Unhook(id);
    m_hookIds.clear();
}
