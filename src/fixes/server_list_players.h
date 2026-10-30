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

// The server browser lists a server's players from what the game server
// reported to the Steam API, and CS2 reports nothing. Every update_interval
// seconds, every connected player's SteamID, name and score is pushed through
// ISteamGameServer::BUpdateUserData (Source2ZE/ServerListPlayersFix).
#pragma once

#include "fix.h"

#include <cstdint>
#include <unordered_set>

class CServerListPlayersFix final : public CFix, public IServerListPlayersControl
{
public:
    const char* GetName() const override { return "server_list_players"; }
    const char* GetDescription() const override
    {
        return "Pushes every connected player's SteamID, name and score to the Steam\n"
               "game server API every update_interval seconds, so the server browser\n"
               "actually lists the players (Source2ZE/ServerListPlayersFix).";
    }
    const char* GetConfigDefaults() const override
    {
        return R"json(
        "update_interval": 5,  // seconds (at least 1)
        "hide": { }            // label -> SteamID64: accounts never pushed (a hidden admin, ...)
)json";
    }

    void ReadConfig(const FixConfig& config) override;
    bool Load(char* error, size_t maxlen) override;
    void Unload() override;

    // IServerListPlayersControl (IGameFixes::ServerListPlayersHide / Show)
    bool HidePlayer(uint64_t steamId64) override;
    bool ShowPlayer(uint64_t steamId64) override;

private:
    void UpdatePlayers();

    // Seconds between pushes ("update_interval").
    float m_flUpdateInterval = 5.0f;

    // The game server's Steam API, usable from GameServerSteamAPIActivated on.
    bool m_bSteamAPIReady = false;

    // SteamID64s left out of the push: "hide" in game_fixes.json plus what
    // plugins add at runtime. The set is only touched from the game thread.
    std::unordered_set<uint64_t> m_Hidden;
    std::unordered_set<uint64_t> m_ConfigHidden;
};
