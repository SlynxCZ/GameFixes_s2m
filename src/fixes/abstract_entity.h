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
// game_player_equip / map entities above all. Creation from keyvalues --
// map spawn, game_player_equip, CreateEntityByName -- is refused for them the
// way an unknown classname is: no entity.
#pragma once

#include "fix.h"

#include <string>
#include <unordered_set>

class CAbstractEntityFix final : public CFix
{
public:
    const char* GetName() const override { return "abstract_entity"; }
    const char* GetDescription() const override
    {
        return "The game allocates any registered classname but only constructs a class its schema allows; anything else\n"
               "(weapon_cs_base in old map ports and their game_player_equip) crashes the server on a null vtable.\n"
               "Creating such an entity fails like an unknown classname.";
    }

    bool Load(char* error, size_t maxlen) override;
    void Unload() override;

private:
    // Each refused classname is logged once.
    std::unordered_set<std::string> m_Logged;
};
