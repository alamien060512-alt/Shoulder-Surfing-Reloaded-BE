# Shoulder Surfing Reloaded (Bedrock)

An over-the-shoulder, RPG-style third-person camera for **Minecraft: Bedrock Edition (Windows)**, inspired by the Java mod [Shoulder Surfing Reloaded](https://www.curseforge.com/minecraft/mc-mods/shoulder-surfing-reloaded). It is a native mod DLL (MinHook, structured like Midnight Client) that nudges the game's third-person render camera sideways, up and back, and turns it slightly so the crosshair still lands where the player aims.

> [!WARNING]
> **Experimental.** Bedrock has no mod API, so this hooks game internals by address. The built-in profile targets **Minecraft 1.26.52.3 only**; on any other build the hook refuses to install and the game is left untouched. The camera write path has been unit-tested for math, but not validated in a live game by the author — see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#validation-status). Not affiliated with Mojang or Microsoft. Do not use on servers that prohibit client modifications.

## Install (mods folder, no injector)

Works like [vibrant-visuals-patcher](https://github.com/th4llium/vibrant-visuals-patcher): a DLL in the mods folder, loaded by [QYCottage/ModLoader](https://github.com/QYCottage/ModLoader) (`WINHTTP.dll`).

1. Install ModLoader. Easiest: run `VibrantVisualPatcherInstaller.exe` from the vibrant-visuals-patcher releases once (it installs ModLoader), or install ModLoader by hand.
2. Download `shoulder-surfing-reloaded.dll` from this repo's Actions run / Releases.
3. Copy the `.dll` itself (not the downloaded .zip, not inside a subfolder) to the mods folder ModLoader reads, `%APPDATA%\Minecraft Bedrock\mods` (`Minecraft Bedrock Preview\mods` for Preview). Copying to the game's own `mods` folder as well is harmless:
   - `%APPDATA%\Minecraft Bedrock\mods`
   - the game's `mods` folder (e.g. `C:\XboxGames\Minecraft for Windows\Content\mods` or `C:\Program Files\WindowsApps\Microsoft.MinecraftUWP_...\mods`)
4. Start Minecraft. ModLoader's debug console must list `Loading plugin: shoulder-surfing-reloaded.dll`, followed by `[shoulder-surfing-reloaded] ... starting; Minecraft <version>`. Join a world (built-in profile: 1.26.52.3), join a world, switch to **back third person**.

It should coexist with vibrant-visuals-patcher and BetterRenderDragon (it only hooks the world camera render call).

| Key (while Minecraft is focused) | Action |
| --- | --- |
| `F6` | Toggle the shoulder camera |
| `F7` | Swap left/right shoulder |
| `F9` | Unload the DLL (game keeps running) |

Settings: `%APPDATA%\Minecraft Bedrock\mods\shoulder-surfing-reloaded.cfg` (created on first run, hot-reloaded when saved):

```ini
enabled=1
left_shoulder=0
side_offset=0.75       # blocks, 0..3
height_offset=0.15     # blocks, -1..2
extra_distance=0.5     # blocks further back, 0..4
focus_distance=24.0    # crosshair converges this far ahead of the player
native_distance=4.0    # vanilla third-person distance (wall-collision heuristic)
transition_speed=12.0  # smoothing for swap/toggle
only_third_person_back=1
toggle_key=0x75
swap_key=0x76
unload_key=0x78
```

Log: `%TEMP%\shoulder-surfing-reloaded.log` (look for `[camera] frames=... applied=... overwritten=...`).

### Antivirus warnings

The DLL is unsigned and hooks game code, so some scanners may still flag it. This project ships **no injector** (no remote threads, no process injection) and releases are built by MSVC in public CI; compare the `SHA256.txt` in the artifact with your file, or build it yourself.

## Build

CI (`.github/workflows/build.yml`) builds with MSVC on `windows-latest` and cross-compiles with MinGW on Linux, running the unit tests on both.

```powershell
# Windows, Visual Studio 2022+ / Build Tools
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

```bash
# Linux/macOS cross-compile
sudo apt install g++-mingw-w64-x86-64 cmake
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build
```

Output: `shoulder-surfing-reloaded.dll` (use the MSVC build for releases; the MinGW build is a CI compile check).

## Not included (yet)

Player-model hiding in first person, real block collision for the camera (only a distance-based shrink heuristic), a crosshair/aim-assist overlay, and profiles for other game versions. MinHook 1.3.3 (BSD-2) is vendored in `third_party/`.
