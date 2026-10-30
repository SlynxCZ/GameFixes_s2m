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

// With tv_enable 1 the GOTV client is never let go: its
// CServerSideClient::Disconnect is superseded and the session's maxplayers is
// raised by one at StartupServer (Pre, so the config is still editable), so
// the slot is its own and the demo keeps recording across match restarts on
// the same map.
#pragma once

#include "fix.h"

#include <engine2/serversideclientbase.h>

class CDemoRecordFix final : public CFix
{
public:
    const char* GetName() const override { return "demo_record"; }
    const char* GetDescription() const override;

    bool Load(char* error, size_t maxlen) override;
    void Unload() override;

    void OnStartupServer(StartupServerContext& ctx) override;

private:
    // CServerSideClient::Disconnect (engine2 vtable slot), Pre.
    HookReturn<void> OnDisconnect(FunctionContext& ctx, bool post);

    // tv_enable, read live at every use: both the disconnect block and the
    // extra slot are no-ops while it's off.
    bool IsTvEnabled();

    uint16 m_nTvEnable = SOURCE2MOD_INVALID_CONVAR_INDEX;
};
