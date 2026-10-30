# Porting a fix into GameFixes_s2m

One fix = one block in `game_fixes.json` + one class in `src/fixes/<name>.h/.cpp`. Port from `git -C /home/GameFixes_mm show HEAD:src/fixes/<name>.cpp` (its working tree has unrelated uncommitted edits). Everything below is verified to compile against `/home/Source2Mod-sdk` (plugin API 7).

## Steps

1. `src/fixes/<name>.h/.cpp`, class `CXxxFix final : public CFix` (`#include "fix.h"`; that pulls in the whole Source2Mod API, the `g_pSource2Mod*` globals, `utils.h`, `vprof.h`, `config.h`). Copy the license header from `src/plugin.cpp`, `Project: GameFixes_s2m`. The build globs `src/**/*.cpp`: no CMake/AMBuild edit.
2. `src/plugin.cpp`: one `#include "fixes/<name>.h"` and one `fixes.push_back(std::make_unique<CXxxFix>());` in `CreateFixes()`, both alphabetical (the only shared lines; keep them trivially mergeable).
3. New gamedata (signatures, offsets, "function" objects) goes in `configs/addons/source2mod/gamedata/game_fixes.json`, see below.
4. steam_ban only: `class CSteamBanFix final : public CFix, public ISteamBanControl` (fix.h) and implement `AllowAccount/DisallowAccount`; `IGameFixes` already forwards to it by name.
5. Build and read the console log (below).

## CFix (src/fixes/fix.h)

`GetName()` (= block name = log prefix) · `GetDescription()` · `GetConfigDefaults()` · `ReadConfig(const FixConfig&)` · `Load(error, maxlen)` · `Unload()` · `OnStartupServer(StartupServerContext&)` · `OnGameFrame(bool simulating)`.
Constructor: members only, nothing else (the plugin makes every fix to collect config defaults, then drops the disabled ones). `Load` failing = that fix is logged "NOT loaded" and off, the rest keep going. Helpers that remember what they made and undo it for you after `Unload()`: `Hook()`, `HookEvent()`, `HookConVarChange()`, `AddRepeatingTimer()`. Only undo by hand what you did yourself (a code patch, an `AddEntityListener`). `FindFunction(name, error, maxlen)`, `FindOffset(...)` (refuses the `-1` that `GAMECONFIG_OFFSET` returns for a missing entry; fed into a vtable slot it corrupts memory), `FindAddress(...)` (= `GAMECONFIG_RESOLVE`) fill `error` for you. `Log("fmt", ...)` prints `[Game Fixes] <name>: ...` (newline added; `g_pSource2ModAPI->Log` itself adds none). `GF_Log()` for plugin-level lines.

## Config (game_fixes.json)

The core owns the file (`ISource2ModConfigs`): `addons/source2mod/configs/Game Fixes/game_fixes.json` (the directory is the plugin's display name), created from the defaults on the first start, `.example.json` rewritten on every load, keys a newer plugin adds are merged in. Comments are allowed. Each fix declares its block's defaults as JSON text, the plugin adds `"enable": true`:

```cpp
const char* GetConfigDefaults() const override { return R"json(
    // seconds between empty-server checks
    "reload_interval": 1800,
    "whitelist": { }     // label -> SteamID64
)json"; }   // no braces, no trailing comma, no "enable"
void ReadConfig(const FixConfig& c) override {
    m_flInterval = c.GetFloat("reload_interval", 1800.0f);
    for (auto& [label, id] : c.GetStringMap("whitelist")) m_whitelist.insert(strtoull(id.c_str(), nullptr, 10));
}
```
`FixConfig`: `GetBool/GetInt/GetInt64/GetUInt64 (number or "quoted")/GetFloat/GetDouble/GetString(key, default)`, `Has`, `Section(key)` (nested object), `GetStringList(key)`, `GetStringMap(key)` (file order), and `Count/KeyAt/At/AsXxx` to walk any shape. **Copy values inside `ReadConfig`**, the nodes are freed after it. Semantics change from the ini: blocks always exist (merged from defaults), off = `"enable": false`. The file is read at plugin load only (a later edit just logs "reload the plugin"). A file that does not parse = plugin loads, every fix off, reason logged.

## Hooks (ISource2ModHooks, never raw KHook)

The plugin has ONE GameFrame (post) and ONE StartupServer (pre) hook and forwards to `OnGameFrame` / `OnStartupServer`; do not add your own for those. `StartupServerContext { const GameSessionConfiguration_t* config; ISource2WorldSession* session; const char* mapName; }`: the config is the engine's own object and the engine reads it after this pre hook, so it is editable via `const_cast<GameSessionConfiguration_t&>(*ctx.config).set_max_clients(n + 1)` (what demo_record needs; compiled).

`Hook(HookPoint::X, HookTarget::All(), handler, post=false, filter={})`; the handler is a lambda, a function or `{ this, &CXxxFix::Method }`, signature `HookReturn<R>(Context&, bool post)`, returning `Action::Ignore / Override / Supersede` (or `{ Action::Supersede, value }`). **R is the context's `Return`**: `MovementContext` (WaterMove, ProcessMovement, ...) answers `HookReturn<bool>` even for void functions, `AcceptInputContext` bool, `TakeDamage` int64, most others void. Pre and post are separate registrations. Returns 0 when the point does not exist on this game: fail `Load` on it. Pointer arguments (move data, damage info) are edited in place; scalar ones need `HookPoint::Function` + `ctx.SetArg`. `ctx.CallOriginal()` runs the game's function; answer `Supersede` after it.

Existing points (use these, they carry the core's signatures): `TryPlayerMove, WaterMove, CategorizePosition, ProcessMovement` (+ the other CCSPlayer_MovementServices functions), `AcceptInput` (filter `{designer, input}`), `TakeDamage, Touch, DropWeapon, PlayerChat, ...`; entity virtuals `Spawn, Use, StartTouch, EndTouch, Blocked, Think, Killed, WeaponPickup, WeaponSwitch` (Instance/Class targets only); engine callbacks `GameFrame, StartupServer, ClientConnect, ClientConnected, ClientPutInServer, ClientDisconnect, ClientVoice, ClientSettingsChanged, GameServerSteamAPIActivated/Deactivated, LoadEventsFromFile`. Game events: `HookEvent("name", [](IGameEvent*, bool post, bool& dontBroadcast) -> Action {...}, post)`; it returns 0 for an event the game has not registered yet, so a fix loaded that early retries from a `LoadEventsFromFile` post hook (no current fix needs events; `OnGameEventManagerReady` is gone).

**Any other function** = a gamedata entry with a `"function"` object, then `Hook(HookPoint::Function(fn), HookTarget::All(), [](FunctionContext& ctx, bool post) -> HookReturn<void> {...})`; `ctx.This()`, `GetArg<T>(i)` (`this` is 0), `SetArg`, `GetReturn/SetReturn`. Not hookable: functions returning a struct by value (`vector_value`, `command`, `buffer_string`).

**A virtual of an engine class by RTTI name** (e.g. `CServerSideClient` in engine2, what demo_record/voice_flood hooked via `GetVirtualTableByName`): the slot comes from gamedata `offsets`, the vtable from `FromVirtualTable`; target is `All()` (the function carries the vtable, `Class()` would only filter, and looks classes up in libserver):

```cpp
int slot;
if (!FindOffset("CServerSideClientBase::Disconnect", slot, error, maxlen)) return false;
static const Source2ModType kParams[] = { Source2ModType::Pointer, Source2ModType::Int32, Source2ModType::String };
const FunctionTypes types{ Source2ModType::Void, kParams, 3, true };    // true: first param is `this`
ISource2ModFunction* fn = g_pSource2ModFunctions->FromVirtualTable(g_PluginID, "engine2", "CServerSideClient", slot, types);
if (!fn) { snprintf(error, maxlen, "CServerSideClient vtable not found"); return false; }
Hook(HookPoint::Function(fn), HookTarget::All(), [this](FunctionContext& ctx, bool post) -> HookReturn<void> {
    return ctx.GetArg<int>(1) == 0 ? Action::Supersede : Action::Ignore; });   // the fn is freed with the plugin
```
A function by signature without gamedata: `FromSignature(g_PluginID, "server", "55 48 ...", types)`.

## Gamedata

`configs/addons/source2mod/gamedata/game_fixes.json` is installed next to the core's `gamedata.json` and read with it. Plain JSON (**no comments**), one object, `name -> entry`; an entry has any of:
`"signatures": { "library": "server", "windows": "48 89 5C ? ...", "linux": "55 48 ..." }` (`?` wildcard; a linux value `"@_ZN...symbol"` is an exported symbol) · `"offsets": { "windows": 12, "linux": 13 }` (vtable slot or field offset) · `"function": { "this": "entity|object|pointer" (omit = free function), "return": "void", "params": ["entity", "int32"] }` (types: void bool int8 int16 int32 int64 float double entity object string vector vector_value pointer buffer_string command int_ref bool_ref float_ref; params may be `{ "windows": [...], "linux": [...] }`; with `offsets` and no signature it is a virtual) · `"patches": { "windows": "EB", "linux": "EB" }` (any text per platform, e.g. replacement bytes: `ParseHexBytes(GAMECONFIG_PATCH(name))`). Library names: `server`, `engine2`, `tier0`, `vscript`. **A name is defined once: the first file wins** (`gamedata.json`, then alphabetical), so never redefine a core name here, look first: `grep '"Name"' /home/Source2Mod/configs/addons/source2mod/gamedata/gamedata.json`.

Already in the core, reuse them: `CCSPlayer_MovementServices::{TryPlayerMove, WaterMove, CategorizePosition, ProcessMovement, ...}` and `CEntityIdentity::AcceptInput` (via HookPoints), `CEntityInstance::AcceptInput` (function), `UTIL::CreateEntityByName` (use `g_pSource2ModEntities->CreateEntityByName`), `IGameEventManager2::LoadEventsFromFile`, `ISource2Server::GameServerSteamAPIActivated/Deactivated`, `ISource2Server::GameFrame`, `INetworkServerService::StartupServer`, `ISource2GameClients::ClientVoice`, `CServerSideClientBase::SendNetMessage` (offset), `CBaseEntity::Teleport/Spawn/Think/...`, `CCSGameRules::TerminateRound`. NOT in the core, to add: CServerSideClient Disconnect/ProcessVoiceData slots, CBeam::SetBeamOrigin/SetBeamEndPos, CBaseFilter::InputTestActivator, SetSchemaHammerUniqueId, GameSystem_Think_CheckSteamBan, CEntitySystem::CreateEntityFromKeyValues, CNavPhysicsInterface, anything else the mm fix scans with `FindPattern`. A code patch: `FindAddress("name", ...)` -> save the original byte(s) -> `WriteCode(ptr, bytes, n)`; undo in `Unload()`. SDK schema classes are `<source2mod/sdk/schema/classes/CBeam.h>` etc.; mm's `src/sdk/game/server/gcbaninformation.h` and `navlib/navphysicsinterface.h` are NOT in the SDK, copy them into `src/sdk/`.

## Time, timers, convars, profiling

* `GameTime()` (utils.h): game seconds as a double, curtime summed per frame, survives map changes. **Never `steady_clock`** for game logic (steam_ban hold_seconds, slow_animation's map age). `CurTime()` is the raw float curtime.
* Timers: `AddRepeatingTimer(sec, [this]{...})` (tracked). One-shot or map-bound: `g_pSource2ModScheduler->AddTimer(sec, cb, flags)`, keep no pointer to it (it is freed after it fires). Next frame: `g_pSource2ModScheduler->NextFrame(cb)`.
* ConVars: `uint16 i = CVAR_IDX("tv_enable"); i != SOURCE2MOD_INVALID_CONVAR_INDEX && CVAR_GET_BOOL(i)`; `UTIL_SetConVar("name", v)`; changes: `HookConVarChange(lambda)`.
* `GF_VPROF("GameFixes::<fix>::<function>")` first line of every handler/timer (names <= 52 chars). Entities/game rules: `g_pSource2ModEntities` (`GetGameRules()`, `FindEntityByClassname`, ...).

## Build

```bash
export SOURCE2MOD_SDK=/home/Source2Mod-sdk        # S2SDK / HL2SDKCS2 already point at /home/s2sdk
cd /home/GameFixes_s2m    # your own copy/worktree: never share a build dir between parallel agents
cmake -S . -B cmake-build-relwithdebinfo-wsl -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_LINKER=/usr/bin/ld.lld-18
cmake --build cmake-build-relwithdebinfo-wsl      # ~20 s first time, seconds after
```
Result: `<build>/addons/source2mod/plugins/game_fixes.smx` and `<build>/addons/source2mod/gamedata/game_fixes.json`; copy both into the server's `addons/source2mod/`. CI uses AMBuild (`-Werror`; same sources, `package/cs2/addons/...`). On a server: `s2m list`, the load log shows each fix `loaded` / `NOT loaded -- reason` / `disabled`.
