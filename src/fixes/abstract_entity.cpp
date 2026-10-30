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
#include "abstract_entity.h"

#include <entity2/entityclass.h>
#include <entity2/entitykeyvalues.h>
#include <entity2/entitysystem.h>
#include <schemasystem/schemasystem.h>

namespace
{
    // What the entity system checks before it runs a class's constructor.
    bool IsConstructible(const CSchemaClassInfo* pInfo)
    {
        // The binding the game reads the flags from is the primary one.
        if (pInfo && pInfo->m_pPrimaryBinding)
            pInfo = pInfo->m_pPrimaryBinding;
        if (!pInfo)
            return true; // nothing to go by: leave it to the game

        constexpr uint32 kMask = SCHEMA_CF1_IS_ABSTRACT | SCHEMA_CF1_CONSTRUCT_ALLOWED | SCHEMA_CF1_CONSTRUCT_DISALLOWED;
        return (pInfo->m_nClassFlags1 & kMask) == SCHEMA_CF1_CONSTRUCT_ALLOWED;
    }
}

bool CAbstractEntityFix::Load(char* error, size_t maxlen)
{
    // CEntityInstance* CEntitySystem::CreateEntityFromKeyValues(CEntityKeyValues* pKeyValues, SpawnGroupHandle_t hSpawnGroup)
    // -- the function logging "CEntitySystem::PrecacheEntity: Classname missing from entity!".
    ISource2ModFunction* pCreate = FindFunction("CEntitySystem::CreateEntityFromKeyValues", error, maxlen);
    if (!pCreate)
        return false;

    const Source2ModHookId id = Hook(HookPoint::Function(pCreate), HookTarget::All(),
        [this](FunctionContext& ctx, bool post) -> HookReturn<void>
        {
            GF_VPROF("GameFixes::abstract_entity::CreateEntityFromKeyValues");

            CEntitySystem* pThis = ctx.GetArg<CEntitySystem*>(0);
            CEntityKeyValues* pKeyValues = ctx.GetArg<CEntityKeyValues*>(1);
            if (!pThis || !pKeyValues)
                return Action::Ignore;

            const char* pszClassname = pKeyValues->GetString("classname", nullptr);
            if (!pszClassname || !*pszClassname)
                return Action::Ignore;

            CEntityClass* pClass = pThis->FindClassByDesignName(pszClassname);
            if (!pClass || !pClass->m_pClassInfo || IsConstructible(pClass->m_pClassInfo->m_pSchemaBinding))
                return Action::Ignore;

            if (m_Logged.insert(pszClassname).second)
            {
                const char* pszCPPName = pClass->m_pClassInfo->m_pszCPPClassname;
                Log("refused to create '%s' (%s cannot be constructed; the game would crash on it)", pszClassname, pszCPPName ? pszCPPName : "?");
            }

            ctx.SetReturn<CEntityInstance*>(nullptr);
            return Action::Supersede;
        });
    if (!id)
    {
        snprintf(error, maxlen, "could not hook CEntitySystem::CreateEntityFromKeyValues");
        return false;
    }

    Log("hooked CEntitySystem::CreateEntityFromKeyValues (%p)", pCreate->GetAddress());
    return true;
}

void CAbstractEntityFix::Unload()
{
    m_Logged.clear();
}
