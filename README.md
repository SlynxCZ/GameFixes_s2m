# GameFixes_s2m

One [Source2Mod](https://source2mod.net) plugin for CS2 server fixes, each toggled in `game_fixes.json`: the Source2Mod port of [GameFixes_mm](https://github.com/SlynxCZ/GameFixes_mm). The plugin is `game_fixes` (`game_fixes.smx`).

**Status: all 13 fixes of GameFixes_mm are ported** (abstract_entity, beam_crash, demo_record, hammer_id, input_activator_crash, rampbug, server_list_players, slow_animation, steam_ban, sv_cheats, team_limit, voice_flood, water_jump) plus `cstv_bot_quota`.

## Install

Drop `addons/source2mod/plugins/game_fixes.smx` and `addons/source2mod/gamedata/game_fixes.json` of a release next to the other Source2Mod files. On the first start Source2Mod writes `addons/source2mod/configs/game_fixes/game_fixes.json` with one block per fix; `"enable": false` turns a fix off. The file is read when the plugin loads.

## For other plugins

`public/gamefixes/IGameFixes.h` (`IGameFixes002`: `SteamBanAllowAccount` / `SteamBanDisallowAccount`, `ServerListPlayersHide` / `ServerListPlayersShow`):

```cpp
#include "gamefixes/IGameFixes.h"

IGameFixes* g_pGameFixes = nullptr;

void MyPlugin::OnAllSource2ModPluginsLoaded()
{
    int ret;
    GET_SOURCE2MOD_IFACE(g_pGameFixes, IGameFixes, GAMEFIXES_INTERFACE_VERSION, ret);
}
```

A call to a fix that is off returns false.

## Build

Against the Source2Mod SDK (`SOURCE2MOD_SDK`), same as the other Source2Mod plugins:

```bash
# CMake
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
# AMBuild (what the CI uses)
mkdir build && cd build && python ../configure.py --enable-optimize --sdks cs2 && ambuild
```

A line appended to `pushbuild.txt` on `main` makes the CI cut the next tag and publish the Linux and Windows archives.

## License

GPLv3, with the same linking exception as Source2Mod. See `LICENSE`.

## SourcePawn

```sourcepawn
#include <gamefixes/gamefixes>

GameFixes.ServerListPlayersHide("76561197960287930");
```

To compile against it, add this repo's `public/scripting` to spcomp's include paths, or use the `scripting/include` folder that comes with a release.
