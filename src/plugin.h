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

#include "common.h"
#include "IGameFixes.h"

#include <memory>
#include <vector>

class CFix;

/**
 * The plugin: loads game_fixes.json, makes the fixes it turns on, owns the two
 * engine hooks every fix shares (GameFrame post, StartupServer pre) and serves
 * IGameFixes to other plugins. The fixes themselves are src/fixes/<name>.*.
 */
class GameFixesPlugin final : public ISource2ModPlugin,
                              public ISource2ModListener,
                              public ISource2ModConfigListener,
                              public IGameFixes
{
public:
    bool Load(PluginId id, ISource2ModAPI* api, char* error, size_t maxlen, bool late) override;
    bool Unload(char* error, size_t maxlen) override;

    // ISource2ModListener
    void* OnSource2ModQuery(const char* iface, int* ret) override;

    // ISource2ModConfigListener
    void OnDeclareConfigs(ISource2ModConfigDeclarations* configs) override;
    void OnConfigChanged(ISource2ModConfig* config, ConfigChange reason) override;

    // IGameFixes
    bool SteamBanAllowAccount(uint64_t steamId64) override;
    bool SteamBanDisallowAccount(uint64_t steamId64) override;

    const char* GetAuthor() override;
    const char* GetName() override;
    const char* GetDescription() override;
    const char* GetVersion() override;

private:
    // The shared engine hooks, forwarded to the loaded fixes.
    HookReturn<void> OnGameFrame(GameFrameContext& ctx, bool post);
    HookReturn<void> OnStartupServer(StartupServerContext& ctx, bool post);

    std::vector<std::unique_ptr<CFix>> CreateFixes();
    /// A loaded fix by its name, or null.
    CFix* FindFix(const char* pszName) const;
    void LoadFixes();

    /// Every fix at first, then the ones game_fixes.json turned on, then (after LoadFixes()) the ones that loaded.
    std::vector<std::unique_ptr<CFix>> m_fixes;
    std::vector<Source2ModHookId> m_hookIds;
};

extern GameFixesPlugin g_Plugin;
