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
#pragma once

// The one include of every source file in this plugin (the fixes too): the
// Source2Mod API with every subsystem header, and the extern declarations of
// the globals SOURCE2MOD_EXPOSE (plugin.cpp) defines -- g_pSource2ModHooks,
// g_pSource2ModFunctions, g_pSource2ModGameConfig, g_pSource2ModConVars,
// g_pSource2ModScheduler, g_pSource2ModEvents, g_pSource2ModAPI, g_PluginID ...
//
// SOURCE2MOD_SAVEVARS() fills one global per subsystem, so every subsystem
// header has to be in scope for its interface name.

#include <source2mod/ISource2ModApi.h>
#include <source2mod/ISource2ModPlugin.h>

#include "source2mod/ISource2ModCommands.h"
#include "source2mod/ISource2ModConVars.h"
#include "source2mod/ISource2ModConfigs.h"
#include "source2mod/ISource2ModEntities.h"
#include "source2mod/ISource2ModEvents.h"
#include "source2mod/ISource2ModFunctions.h"
#include "source2mod/ISource2ModGameConfig.h"
#include "source2mod/ISource2ModGameSystems.h"
#include "source2mod/ISource2ModHookContexts.h"
#include "source2mod/ISource2ModHooks.h"
#include "source2mod/ISource2ModJSON.h"
#include "source2mod/ISource2ModMemory.h"
#include "source2mod/ISource2ModModule.h"
#include "source2mod/ISource2ModNetworkMessages.h"
#include "source2mod/ISource2ModScheduler.h"
#include "source2mod/ISource2ModTrace.h"

#include "utils.h"
#include "vprof.h"

SOURCE2MOD_GLOBALVARS();
