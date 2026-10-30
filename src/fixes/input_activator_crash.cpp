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
#include "input_activator_crash.h"

bool CInputActivatorCrashFix::Load(char* error, size_t maxlen)
{
    // CEntityIdentity::AcceptInput(name, activator, caller, value, ...), the core's AcceptInput point. Its filter
    // compares designer and input names exactly (case-sensitive) and the filters are a family ("filter_*"), so
    // every input comes here and the names are compared in the handler, case-insensitively as the game does.
    const Source2ModHookId id = Hook(HookPoint::AcceptInput, HookTarget::All(),
        [](AcceptInputContext& ctx, bool post) -> HookReturn<bool>
        {
            GF_VPROF("GameFixes::input_activator_crash::AcceptInput");

            // If null activator (player disconnected & pawn removed), block the real TestActivator function from
            // executing and crashing the server.
            if (ctx.activator || !ctx.identity || !ctx.inputName)
                return Action::Ignore;

            const char* pszInput = ctx.inputName->String();
            const char* pszClassname = ctx.identity->GetClassname();
            if (pszInput && pszClassname && !V_strnicmp(pszClassname, "filter_", 7) && !V_strcasecmp(pszInput, "TestActivator"))
                return { Action::Supersede, true };

            return Action::Ignore;
        });
    if (!id)
    {
        snprintf(error, maxlen, "the AcceptInput hook point is not available");
        return false;
    }

    Log("hooked CEntityIdentity::AcceptInput");
    return true;
}
