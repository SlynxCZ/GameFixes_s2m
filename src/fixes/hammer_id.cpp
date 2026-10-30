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
#include "hammer_id.h"

bool CHammerIdFix::Load(char* error, size_t maxlen)
{
    // https://github.com/Source2ZE/CS2Fixes/commit/61937f78dd649ed391f6988b0c58ae4a75fd4bc6

    // The gamedata pattern starts on the jnz itself (75 ?); its "patches" bytes (EB) turn it into a jmp.
    const ISource2ModMemory address = FindAddress("SetSchemaHammerUniqueId", error, maxlen);
    if (!address)
        return false;

    const char* pszPatch = GAMECONFIG_PATCH("SetSchemaHammerUniqueId");
    const std::vector<uint8_t> patch = ParseHexBytes(pszPatch ? pszPatch : "");
    if (patch.empty())
    {
        snprintf(error, maxlen, "gamedata \"SetSchemaHammerUniqueId\" has no usable \"patches\" entry for this platform");
        return false;
    }

    void* pTarget = address.GetPtr();
    const uint8_t* pBytes = static_cast<const uint8_t*>(pTarget);
    m_original.assign(pBytes, pBytes + patch.size());

    if (!WriteCode(pTarget, patch.data(), patch.size()))
    {
        m_original.clear();
        snprintf(error, maxlen, "could not write the patch at %p", pTarget);
        return false;
    }

    m_pPatch = pTarget;
    Log("patched SetSchemaHammerUniqueId at %p (jnz -> jmp)", pTarget);
    return true;
}

void CHammerIdFix::Unload()
{
    if (!m_pPatch)
        return;

    WriteCode(m_pPatch, m_original.data(), m_original.size());
    m_pPatch = nullptr;
    m_original.clear();
}
