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
// What every fix looks like to the plugin.
#pragma once

#include "common.h"
#include "config.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class CFix
{
public:
    /// Constructors do nothing but set members: no hooks, no gamedata, no logs.
    /// The plugin makes every fix to ask it for its config defaults, then
    /// destroys the ones the config leaves off. Everything real is in Load().
    virtual ~CFix() = default;

    /// Key of this fix's block in game_fixes.json; also its prefix in the log.
    virtual const char* GetName() const = 0;

    /// One line of what it fixes; becomes a comment above the block in the
    /// generated game_fixes.json / game_fixes.example.json (split on '\n').
    virtual const char* GetDescription() const { return ""; }

    /// Whether the generated game_fixes.json starts with this fix on. True for
    /// all of them but one that only matters on some servers.
    virtual bool EnabledByDefault() const { return true; }

    /// The fix's own options and their defaults: the *members* of its block as
    /// JSON text, comments allowed, without the braces and without "enable"
    /// (the plugin adds `"enable": true` itself). Written to the admin's
    /// file on the first start, and merged into it afterwards when a key is
    /// missing, so ReadConfig() always finds every key declared here.
    ///
    ///   return R"json(
    ///       // seconds between empty-server checks
    ///       "reload_interval": 1800,
    ///       "whitelist": { }
    ///   )json";
    virtual const char* GetConfigDefaults() const { return ""; }

    /// Reads the fix's options out of its block. Only called when the block
    /// has "enable": true, right before Load(). Copy the values: see config.h.
    virtual void ReadConfig(const FixConfig& config) {}

    /// Installs the fix. On failure write why into error and return false; the
    /// plugin logs it, calls Unload() and leaves just this fix off, the others
    /// still load. Hooks, event hooks and repeating timers made through the
    /// helpers below are taken down by the plugin afterwards -- Unload() only
    /// has to undo what the fix did by itself (a code patch, a convar).
    virtual bool Load(char* error, size_t maxlen) = 0;

    /// Called on a failed Load() and when the plugin unloads (for a fix whose Load()
    /// returned false it must cope with whatever Load() left unset).
    virtual void Unload() {}

    /// Forwarded from the plugin's one shared StartupServer hook (pre): a new
    /// map's server is about to come up. The session configuration is still
    /// editable here.
    virtual void OnStartupServer(StartupServerContext& ctx) {}

    /// Forwarded from the plugin's one shared GameFrame hook (post).
    virtual void OnGameFrame(bool simulating) {}

protected:
    /// "<name>: <message>" on the server console, one line, a newline added.
    void Log(const char* pszFormat, ...) const;

    /**
     * g_pSource2ModHooks->Hook(), remembering the id so the plugin can take
     * the hook down when the fix is unloaded. Same arguments, same contract
     * (a handler is a lambda, a function or { this, &CFix::Method }).
     *
     * @return the hook id, 0 when the point is not available on this game.
     */
    template <typename CONTEXT>
    Source2ModHookId Hook(HookPointOf<CONTEXT> point, const HookTarget& target,
                          typename HookNonDeduced<HookHandler<CONTEXT>>::type handler,
                          bool post = false, const HookFilter& filter = {})
    {
        const Source2ModHookId id = g_pSource2ModHooks->Hook(point, target, std::move(handler), post, filter);
        if (id)
            m_hookIds.push_back(id);
        return id;
    }

    /// g_pSource2ModEvents->HookGameEvent(), id remembered. 0 for an event the
    /// game does not have (yet).
    Source2ModEventHookId HookEvent(const char* pszEvent, GameEventHandler handler, bool post = false);

    /// g_pSource2ModConVars->HookConVarChange() (every ConVar's changes; compare
    /// the name), id remembered.
    Source2ModConVarHookId HookConVarChange(ConVarChangeHandler handler);

    /// g_pSource2ModNetworkMessages->HookClientMessage(): every message a client
    /// sends the server, before the engine handles it (the message is the
    /// protobuf; Action::Supersede drops it), id remembered. 0 when the core
    /// cannot hook it.
    Source2ModNetMessageHookId HookClientMessage(NetMessageClientHook handler);

    /// A repeating timer on game time (the scheduler's), killed when the fix is
    /// unloaded. It survives map changes. For a one-shot or a map-bound timer
    /// use g_pSource2ModScheduler->AddTimer directly and keep no pointer to it.
    Timer* AddRepeatingTimer(float flInterval, TimerCallback callback);

    /// A gamedata function (a "function" object in game_fixes.json or the
    /// core's gamedata.json). Null with the reason in error when it is not
    /// there or does not match the running game.
    ISource2ModFunction* FindFunction(const char* pszName, char* error, size_t maxlen) const;

    /// A gamedata offset (a vtable slot, a struct offset). False with the
    /// reason in error when the entry is missing: GAMECONFIG_OFFSET answers -1
    /// for that, and a -1 fed to a hook or an offset silently corrupts memory.
    bool FindOffset(const char* pszName, int& offset, char* error, size_t maxlen) const;

    /// A gamedata address (signature or "@symbol"), resolved to where it is in
    /// memory. Null with the reason in error when the module or the pattern is missing.
    ISource2ModMemory FindAddress(const char* pszName, char* error, size_t maxlen) const;

private:
    friend class GameFixesPlugin;

    /// The plugin calls this after every Unload(), loaded or not.
    void ReleaseResources();

    std::vector<Source2ModHookId> m_hookIds;
    std::vector<Source2ModEventHookId> m_eventHookIds;
    std::vector<Source2ModConVarHookId> m_conVarHookIds;
    std::vector<Source2ModNetMessageHookId> m_netMessageHookIds;
    std::vector<Timer*> m_timers;
};

/**
 * Implemented by the steam_ban fix next to CFix, for the plugin's IGameFixes:
 *
 *   class CSteamBanFix final : public CFix, public ISteamBanControl { ... };
 */
class ISteamBanControl
{
public:
    virtual ~ISteamBanControl() = default;

    virtual bool AllowAccount(uint64_t steamId64) = 0;
    virtual bool DisallowAccount(uint64_t steamId64) = 0;
};

/**
 * Implemented by the server_list_players fix next to CFix, for the plugin's
 * IGameFixes (found by name, like ISteamBanControl).
 */
class IServerListPlayersControl
{
public:
    virtual ~IServerListPlayersControl() = default;

    virtual bool HidePlayer(uint64_t steamId64) = 0;
    virtual bool ShowPlayer(uint64_t steamId64) = 0;
};

/**
 * The defaults of game_fixes.json: one block per fix, from what each fix
 * declares (GetDescription, GetConfigDefaults), `"enable": true` first. Inline
 * so it can be tried without the plugin.
 */
inline std::string BuildFixConfigDefaults(const std::vector<std::unique_ptr<CFix>>& fixes)
{
    std::string text = "// GameFixes -- one block per fix. A fix only does anything with \"enable\": true.\n"
                       "// Everything else in a block is that fix's own options.\n"
                       "{\n";

    for (size_t i = 0; i < fixes.size(); i++)
    {
        const CFix& fix = *fixes[i];

        // The description as comment lines.
        const std::string description = fix.GetDescription();
        size_t start = 0;
        while (!description.empty() && start <= description.size())
        {
            const size_t end = description.find('\n', start);
            text += "    // " + description.substr(start, end == std::string::npos ? std::string::npos : end - start) + "\n";
            if (end == std::string::npos)
                break;
            start = end + 1;
        }

        text += std::string("    \"") + fix.GetName() + "\": {\n        \"enable\": " + (fix.EnabledByDefault() ? "true" : "false");

        // The body without its blank first and last lines (a raw string's).
        std::string body = fix.GetConfigDefaults();
        const size_t last = body.find_last_not_of(" \t\r\n");
        if (last != std::string::npos)
        {
            body.erase(last + 1);
            const size_t first = body.find_first_not_of("\r\n");
            const size_t lineStart = body.rfind('\n', first);
            body.erase(0, lineStart == std::string::npos ? 0 : lineStart + 1);
            text += ",\n" + body;
        }

        text += "\n    }";
        text += (i + 1 < fixes.size()) ? ",\n" : "\n";
    }

    text += "}\n";
    return text;
}
