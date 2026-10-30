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
// SetSchemaHammerUniqueId skips m_sUniqueHammerID behind a jnz.
#pragma once

#include "fix.h"

#include <cstdint>
#include <vector>

class CHammerIdFix final : public CFix
{
public:
    const char* GetName() const override { return "hammer_id"; }
    const char* GetDescription() const override
    {
        return "Patches SetSchemaHammerUniqueId so m_sUniqueHammerID is filled in on every entity (jnz -> jmp).";
    }

    bool Load(char* error, size_t maxlen) override;
    void Unload() override;

private:
    // The patched bytes (the jnz opcode) and what they were before.
    void* m_pPatch = nullptr;
    std::vector<uint8_t> m_original;
};
