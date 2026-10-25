# Architecture

Mirrors the Midnight Client module layout: `core` → `config` → `modules/*` → composition root.

```
src/client/ShoulderSurfing.cpp          DllMain, worker thread, hotkeys, config hot-reload, unload
src/client/core/Logging.*               C:\Users\Public\ShoulderSurfing.log
src/client/config/Settings.*            key=value parsing/clamping (pure, tested)
src/client/modules/camera/CameraMath.*  matrix math (pure, tested)
src/client/modules/camera/NativeCamera.* render hook (version-gated)
src/client/modules/camera/ViewSettings.* reads game_thirdperson from options.txt
src/launcher/Injector.cpp               LoadLibraryW CLI injector
```

## How the camera works

The hook sits on `LevelRendererCamera::render` (RVA `0x4686F20` on 1.26.52.3, found by the Midnight project). Per non-shadow frame it:

1. reads the view object's eye and the camera block's eye (`renderer+0x740+0x124`); a gap > 0.5 block means a chase (third-person) camera;
2. validates that the top of the view-matrix stack (`camera+0x0`) and the inverse (`camera+0xC0`) are a rigid inverse pair — otherwise it writes nothing;
3. builds `V' = Rᵀ · T(−t) · V` and `V'⁻¹ = V⁻¹ · T(t) · R` where `t = (side, up, back)` in camera space and `R` turns the camera so a point `focus_distance` blocks ahead of the head stays on the view axis (so the crosshair matches the aim ray);
4. writes V', V'⁻¹ and the world eye, calls the original render, then **restores** the vanilla values so gameplay code never sees the offset.

Offsets ease with `1 − e^(−rate·dt)` smoothing; they are scaled by `nativeDistance / native_distance` so a wall that pushes the vanilla camera in also shrinks the shoulder offset.

## Safety

- Binary profile check (function prologue + virtual-table slot) before hooking; mismatch ⇒ DLL unloads itself.
- All memory access is through `ReadProcessMemory`/`WriteProcessMemory` on the current process (no faults).
- Unload drains in-flight hook calls before `MH_Uninitialize`.

## Validation status

- Verified here: math (`tests/CameraMathTests.cpp`: crosshair target on the view axis, inverse consistency, mirrored shoulder, zero-offset identity), settings parsing, clean MinGW build.
- **Not verified in a live game:** whether the render call re-derives the camera from the view object (which would overwrite the shifted matrices). The log line `overwritten=N` counts exactly that. If `overwritten` ≈ `applied`, the camera needs to be patched earlier in the frame — open an issue with the log.
- Only 1.26.52.3 has a profile. Updating needs a new RVA/offset set (use Midnight's `Dumper`/`Analyzer` tools).
