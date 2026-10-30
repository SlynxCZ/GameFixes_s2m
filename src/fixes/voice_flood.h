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

// A client can send clc_VoiceData that is malformed (no audio, a sender that
// isn't them, an unknown format, packets announced with no data) or simply
// huge, and the engine does the work of handling every one of them. Those
// messages are dropped in CServerSideClient::ProcessVoiceData before the
// engine sees them, and every client is held to a rolling one-second budget
// of voice work. Nobody is kicked; a dropped message is just never played.
#pragma once

#include "fix.h"

#include <const.h>
#include <engine2/serversideclientbase.h>

#include <cstddef>
#include <cstdint>
#include <deque>

class CVoiceFloodFix final : public CFix
{
public:
    const char* GetName() const override { return "voice_flood"; }
    const char* GetDescription() const override;

    bool Load(char* error, size_t maxlen) override;
    void Unload() override;

    void OnStartupServer(StartupServerContext& ctx) override;

private:
    // CServerSideClient::ProcessVoiceData (engine2 vtable slot), Pre.
    HookReturn<void> OnProcessVoiceData(FunctionContext& ctx, bool post);

    // What one voice message cost, and when (GameTime()).
    struct Work
    {
        double time = 0.0;
        size_t cost = 0;
        uint32_t packets = 0;
    };

    // What one client got through in the last second. A slot taken over by a
    // new client inherits at most one second of someone else's budget, so a
    // disconnect needs no reset of its own.
    struct Budget
    {
        std::deque<Work> recent;
        size_t cost = 0;
        uint32_t packets = 0;
    };

    bool ShouldDrop(CServerSideClientBase* pClient, const CCLCMsg_VoiceData& msg);

    Budget m_Budgets[ABSOLUTE_PLAYER_LIMIT];
    uint64_t m_nDropped = 0;
    bool m_bLoggedFirstDrop = false;
};
