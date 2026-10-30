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
#include "common.h"

#include <engine2/gameshareddefs.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

void GF_Log(const char* pszFormat, ...)
{
    char szMessage[1024];

    va_list args;
    va_start(args, pszFormat);
    std::vsnprintf(szMessage, sizeof(szMessage), pszFormat, args);
    va_end(args);

    // The core adds the "[Game Fixes] " prefix and no newline.
    g_pSource2ModAPI->Log("%s\n", szMessage);
}

static double s_dGameTime = 0.0;
// The curtime of the last frame, 0 before the first.
static double s_dLastCurTime = 0.0;

double GameTime()
{
    return s_dGameTime;
}

float CurTime()
{
    const CGlobalVars* pGlobals = g_pSource2ModAPI ? g_pSource2ModAPI->GetGlobalVars() : nullptr;
    return pGlobals ? pGlobals->curtime : 0.0f;
}

void ResetGameTime()
{
    s_dGameTime = 0.0;
    s_dLastCurTime = 0.0;
}

void TickGameTime(bool simulating)
{
    // curtime restarts at every map, so a step backwards (or the first frame)
    // counts as one frame.
    const CGlobalVars* pGlobals = g_pSource2ModAPI->GetGlobalVars();

    if (simulating && pGlobals)
    {
        const double now = pGlobals->curtime;
        double step = now - s_dLastCurTime;
        if (s_dLastCurTime <= 0.0 || step < 0.0)
            step = pGlobals->frametime > 0.0f ? pGlobals->frametime : 0.015;

        s_dGameTime += step;
        s_dLastCurTime = now;
    }
    else
    {
        s_dGameTime += 0.015;
        if (pGlobals)
            s_dLastCurTime = pGlobals->curtime;
    }
}

bool WriteCode(void* address, const void* pBytes, std::size_t len)
{
    if (!address || !pBytes || !len)
        return false;

#ifdef _WIN32
    DWORD oldProtect = 0;
    if (!VirtualProtect(address, len, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    std::memcpy(address, pBytes, len);
    FlushInstructionCache(GetCurrentProcess(), address, len);

    DWORD ignored = 0;
    VirtualProtect(address, len, oldProtect, &ignored);
    return true;
#else
    // Linux has no mprotect() query to read the old flags back. Code is
    // read + execute (a vtable would be read only, but a patch is never one),
    // so the window goes RWX -> RX.
    const auto pageSize = static_cast<std::uintptr_t>(sysconf(_SC_PAGESIZE));
    const auto begin = reinterpret_cast<std::uintptr_t>(address);
    const std::uintptr_t pageStart = begin & ~(pageSize - 1);
    const std::uintptr_t pageEnd = (begin + len + pageSize - 1) & ~(pageSize - 1);
    void* pPage = reinterpret_cast<void*>(pageStart);

    if (mprotect(pPage, pageEnd - pageStart, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
        return false;

    std::memcpy(address, pBytes, len);
    mprotect(pPage, pageEnd - pageStart, PROT_READ | PROT_EXEC);
    return true;
#endif
}

std::vector<uint8_t> ParseHexBytes(const char* pszText)
{
    std::vector<uint8_t> bytes;
    if (!pszText)
        return bytes;

    int high = -1;
    for (const char* p = pszText; *p; p++)
    {
        if (*p == ' ' || *p == '\t')
        {
            if (high >= 0)
                return {};
            continue;
        }

        int digit;
        if (*p >= '0' && *p <= '9')
            digit = *p - '0';
        else if (*p >= 'a' && *p <= 'f')
            digit = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F')
            digit = *p - 'A' + 10;
        else
            return {};

        if (high < 0)
        {
            high = digit;
        }
        else
        {
            bytes.push_back(static_cast<uint8_t>(high << 4 | digit));
            high = -1;
        }
    }

    if (high >= 0)
        return {};
    return bytes;
}
