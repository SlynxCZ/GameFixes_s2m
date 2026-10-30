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
#include "plugin.h"
#include "version_gen.h"

#include "fixes/fix.h"

#include <cstdio>
#include <cstring>
#include <string>

// One include per fix, alphabetical, as they are ported:
#include "fixes/rampbug.h"
#include "fixes/water_jump.h"

GameFixesPlugin g_Plugin;
SOURCE2MOD_EXPOSE(PLUGIN_NAME, g_Plugin);

bool GameFixesPlugin::Load(PluginId id, ISource2ModAPI* api, char* error, size_t maxlen, bool late)
{
    SOURCE2MOD_SAVEVARS();

    if (!g_pSource2ModHooks || !g_pSource2ModFunctions || !g_pSource2ModGameConfig || !g_pSource2ModConfigs ||
        !g_pSource2ModScheduler || !g_pSource2ModEvents)
    {
        if (error && maxlen)
            g_pSource2ModAPI->Format(error, maxlen, "the core is missing an interface this plugin needs (hooks, functions, gamedata, configs, scheduler, events): update Source2Mod");
        return false;
    }

    ResetGameTime();

    // OnSource2ModQuery.
    api->AddListener(this);

    m_fixes = CreateFixes();

    // game_fixes.json: OnDeclareConfigs() gives the core the defaults, and
    // OnConfigChanged(Loaded) -- before AddListener() returns -- drops the
    // fixes the file leaves off and lets the rest read their options. A file
    // that is missing is written from the defaults; one that does not parse
    // (the config is SOURCE2MOD_CONFIG_REQUIRED) is a reason here, not a
    // failed load: the plugin stays up with every fix off.
    char szConfigError[512] = "";
    if (!g_pSource2ModConfigs->AddListener(this, szConfigError, sizeof(szConfigError)))
    {
        GF_Log("game_fixes.json not usable, every fix is off: %s", szConfigError[0] ? szConfigError : "unknown error");
        m_fixes.clear();
    }

    LoadFixes();

    // The two hooks every fix shares. Nothing to forward to without a fix.
    if (!m_fixes.empty())
    {
        // After the game's frame: the fixes only read it.
        m_hookIds.push_back(g_pSource2ModHooks->Hook(HookPoint::GameFrame, HookTarget::All(), { this, &GameFixesPlugin::OnGameFrame }, true));
        // Before the engine reads the session configuration, so a fix can still edit it.
        m_hookIds.push_back(g_pSource2ModHooks->Hook(HookPoint::StartupServer, HookTarget::All(), { this, &GameFixesPlugin::OnStartupServer }, false));
    }

    GF_Log("%zu fix(es) loaded", m_fixes.size());
    return true;
}

bool GameFixesPlugin::Unload(char* error, size_t maxlen)
{
    // The hooks and timers are dropped by the core after this returns anyway;
    // the fixes' own go first so a fix never runs once it is unloaded.
    for (const Source2ModHookId id : m_hookIds)
    {
        if (id)
            g_pSource2ModHooks->Unhook(id);
    }
    m_hookIds.clear();

    for (auto it = m_fixes.rbegin(); it != m_fixes.rend(); ++it)
    {
        (*it)->Unload();
        (*it)->ReleaseResources();
    }
    m_fixes.clear();

    return true;
}

/*
 * One line per fix, alphabetical -- the ported ones. To come (from GameFixes_mm):
 *   abstract_entity, beam_crash, demo_record, hammer_id, input_activator_crash,
 *   rampbug, server_list_players, slow_animation, steam_ban, sv_cheats,
 *   team_limit, voice_flood, water_jump
 */
std::vector<std::unique_ptr<CFix>> GameFixesPlugin::CreateFixes()
{
    std::vector<std::unique_ptr<CFix>> fixes;
    fixes.push_back(std::make_unique<CRampbugFix>());
    fixes.push_back(std::make_unique<CWaterJumpFix>());
    return fixes;
}

void GameFixesPlugin::OnDeclareConfigs(ISource2ModConfigDeclarations* configs)
{
    // configs/<this plugin's name>/game_fixes.json: one block per fix, built
    // from what each fix declares. Every fix is on by default (the shipped
    // game_fixes.ini of GameFixes_mm had every one on); "enable": false turns
    // one off.
    const std::string text = BuildFixConfigDefaults(m_fixes);

    // REQUIRED: a file that does not parse fails AddListener() (every fix off,
    // see Load()) instead of running on the defaults, which have every fix on.
    configs->Add("game_fixes", text.c_str(), SOURCE2MOD_CONFIG_REQUIRED);
}

void GameFixesPlugin::OnConfigChanged(ISource2ModConfig* config, ConfigChange reason)
{
    if (reason != ConfigChange::Loaded)
    {
        // Hooks and patches are installed by Load(); applying a changed file
        // would mean unloading and loading every fix. The plugin does that.
        GF_Log("%s changed: it is read when the plugin loads (s2m unload / s2m load this plugin)", config->GetName());
        return;
    }

    const ISource2ModJSONValue* pRoot = config->Root();

    // The fixes the file turns on read their options now (the nodes are gone
    // after this call); the rest are dropped. "Missing block = off".
    std::vector<std::unique_ptr<CFix>> enabled;
    for (auto& pFix : m_fixes)
    {
        const ISource2ModJSONValue* pBlock = pRoot ? pRoot->Get(pFix->GetName()) : nullptr;
        if (!pBlock || !pBlock->IsObject() || !pBlock->GetBool("enable", false))
        {
            GF_Log("%s: disabled", pFix->GetName());
            continue;
        }

        pFix->ReadConfig(FixConfig(pBlock));
        enabled.push_back(std::move(pFix));
    }

    m_fixes = std::move(enabled);
}

void GameFixesPlugin::LoadFixes()
{
    std::vector<std::unique_ptr<CFix>> loaded;

    for (auto& pFix : m_fixes)
    {
        char szError[256] = "";
        if (!pFix->Load(szError, sizeof(szError)))
        {
            GF_Log("%s: NOT loaded -- %s", pFix->GetName(), szError[0] ? szError : "unknown error");
            pFix->Unload();
            pFix->ReleaseResources();
            continue;
        }

        GF_Log("%s: loaded", pFix->GetName());
        loaded.push_back(std::move(pFix));
    }

    m_fixes = std::move(loaded);
}

HookReturn<void> GameFixesPlugin::OnGameFrame(GameFrameContext& ctx, bool post)
{
    GF_VPROF("GameFixes::GameFrame");

    TickGameTime(ctx.simulating);

    for (auto& pFix : m_fixes)
        pFix->OnGameFrame(ctx.simulating);

    return Action::Ignore;
}

HookReturn<void> GameFixesPlugin::OnStartupServer(StartupServerContext& ctx, bool post)
{
    GF_VPROF("GameFixes::StartupServer");

    for (auto& pFix : m_fixes)
        pFix->OnStartupServer(ctx);

    return Action::Ignore;
}

CFix* GameFixesPlugin::FindFix(const char* pszName) const
{
    for (const auto& pFix : m_fixes)
    {
        if (!std::strcmp(pFix->GetName(), pszName))
            return pFix.get();
    }

    return nullptr;
}

void* GameFixesPlugin::OnSource2ModQuery(const char* iface, int* ret)
{
    if (!iface)
        return nullptr;

    if (!std::strcmp(iface, GAMEFIXES_INTERFACE_VERSION))
    {
        if (ret) *ret = SOURCE2MOD_IFACE_OK;
        return static_cast<IGameFixes*>(this);
    }

    if (ret) *ret = SOURCE2MOD_IFACE_FAILED;

    return nullptr;
}

bool GameFixesPlugin::SteamBanAllowAccount(uint64_t steamId64)
{
    auto* pControl = dynamic_cast<ISteamBanControl*>(FindFix("steam_ban"));
    return pControl && pControl->AllowAccount(steamId64);
}

bool GameFixesPlugin::SteamBanDisallowAccount(uint64_t steamId64)
{
    auto* pControl = dynamic_cast<ISteamBanControl*>(FindFix("steam_ban"));
    return pControl && pControl->DisallowAccount(steamId64);
}

const char* GameFixesPlugin::GetAuthor()
{
    return PLUGIN_AUTHOR;
}

const char* GameFixesPlugin::GetName()
{
    return PLUGIN_DISPLAY_NAME;
}

const char* GameFixesPlugin::GetDescription()
{
    return PLUGIN_DESCRIPTION;
}

const char* GameFixesPlugin::GetVersion()
{
    return PLUGIN_FULL_VERSION;
}
