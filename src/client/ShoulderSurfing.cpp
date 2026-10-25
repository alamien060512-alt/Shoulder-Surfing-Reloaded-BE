// Shoulder Surfing Reloaded for Minecraft: Bedrock Edition (Windows).
// Composition root: DllMain, hook lifetime, hotkeys and config reload.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <MinHook.h>

#include "client/config/Settings.h"
#include "client/core/Logging.h"
#include "client/modules/camera/NativeCamera.h"
#include "client/modules/camera/ViewSettings.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <cstdio>

namespace {
HMODULE gModule = nullptr;
std::atomic<bool> gUnloadRequested{false};

// Config lives beside the other mods: %APPDATA%\\Minecraft Bedrock\\mods\\shoulder-surfing-reloaded.cfg
// (falls back to %TEMP% when that folder cannot be created).
const std::wstring& ConfigPath() {
    static const std::wstring path = [] {
        wchar_t base[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"APPDATA", base, MAX_PATH) > 0 && GetLastError() != ERROR_ENVVAR_NOT_FOUND) {
            std::wstring dir = std::wstring(base) + L"\\Minecraft Bedrock\\mods";
            CreateDirectoryW((std::wstring(base) + L"\\Minecraft Bedrock").c_str(), nullptr);
            CreateDirectoryW(dir.c_str(), nullptr);
            if (GetFileAttributesW(dir.c_str()) != INVALID_FILE_ATTRIBUTES)
                return dir + L"\\shoulder-surfing-reloaded.cfg";
        }
        wchar_t temp[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, temp);
        return std::wstring(temp) + L"shoulder-surfing-reloaded.cfg";
    }();
    return path;
}

std::string ReadFile(const std::wstring& path) {
    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void WriteFile(const std::wstring& path, const std::string& text) {
    std::ofstream out(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    out << text;
}

std::string GameVersion() {
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return "unknown";
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path, &handle);
    if (!size) return "unknown";
    std::string buffer(size, '\0');
    VS_FIXEDFILEINFO* info = nullptr;
    UINT length = 0;
    if (!GetFileVersionInfoW(path, handle, size, buffer.data()) ||
        !VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<void**>(&info), &length) || !info) return "unknown";
    char out[64];
    snprintf(out, sizeof(out), "%u.%u.%u.%u", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
             HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
    return out;
}

bool GameFocused() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

// Edge-detected key press, only while Minecraft has focus.
struct KeyEdge {
    bool down = false;
    bool Pressed(int vk) {
        const bool now = vk > 0 && GameFocused() && (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool edge = now && !down;
        down = now;
        return edge;
    }
};

DWORD WINAPI Worker(LPVOID) {
    using namespace shoulder;
    LogF("Shoulder Surfing Reloaded (BE) starting; Minecraft %s (built-in profile: 1.26.52.3)", GameVersion().c_str());

    if (MH_Initialize() != MH_OK) {
        LogF("MinHook init failed");
        return 1;
    }
    // Loaded by ModLoader at process start: wait for the game image to settle.
    bool hooked = false;
    for (int attempt = 0; attempt < 60 && !hooked; ++attempt) {
        hooked = camera::CreateNativeCameraHook(/*quiet=*/attempt + 1 < 60);
        if (!hooked) Sleep(1000);
    }
    if (!hooked) {
        LogF("Unsupported Minecraft build; shoulder camera stays disabled (game untouched)");
        MH_Uninitialize();
        return 2;
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        LogF("MH_EnableHook failed");
        camera::DrainNativeCamera();
        MH_Uninitialize();
        return 3;
    }

    const std::string existing = ReadFile(ConfigPath());
    config::Settings settings = config::Parse(existing);
    if (existing.empty()) WriteFile(ConfigPath(), config::Serialize(settings));
    FILETIME lastWrite{};
    auto stamp = [&] {
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (GetFileAttributesExW(ConfigPath().c_str(), GetFileExInfoStandard, &data)) lastWrite = data.ftLastWriteTime;
    };
    stamp();

    KeyEdge toggle, swap, unload;
    int perspective = -1;
    ULONGLONG lastPerspectivePoll = 0, lastConfigPoll = 0, lastReport = 0, lastSave = 0;
    bool dirty = false;
    auto publish = [&] {
        camera::Controls c;
        c.active = settings.enabled;
        c.target = config::ToParams(settings, 1.0f);
        c.nominalDistance = settings.nominalDistance;
        c.transitionSpeed = settings.transitionSpeed;
        c.excludeFrontView = settings.onlyThirdPersonBack;
        c.optionsPerspective = perspective;
        camera::PublishControls(c);
    };
    publish();
    LogF("Ready. F6 toggle, F7 swap shoulder, F9 unload");

    while (!gUnloadRequested.load()) {
        Sleep(16);
        const ULONGLONG now = GetTickCount64();
        bool changed = false;

        if (toggle.Pressed(settings.toggleKey)) { settings.enabled = !settings.enabled; changed = dirty = true; }
        if (swap.Pressed(settings.swapKey)) { settings.leftShoulder = !settings.leftShoulder; changed = dirty = true; }
        if (unload.Pressed(settings.unloadKey)) break;

        if (now - lastPerspectivePoll > 500) {
            lastPerspectivePoll = now;
            const int p = camera::ReadGamePerspective();
            if (p != perspective) { perspective = p; changed = true; LogF("game_thirdperson=%d", p); }
        }
        if (now - lastConfigPoll > 1000) {
            lastConfigPoll = now;
            WIN32_FILE_ATTRIBUTE_DATA data{};
            if (GetFileAttributesExW(ConfigPath().c_str(), GetFileExInfoStandard, &data) &&
                CompareFileTime(&data.ftLastWriteTime, &lastWrite) != 0) {
                lastWrite = data.ftLastWriteTime;
                settings = config::Parse(ReadFile(ConfigPath()));
                changed = true;
                LogF("config reloaded");
            }
        }
        if (dirty && now - lastSave > 1000) {   // persist hotkey changes, debounced
            lastSave = now;
            dirty = false;
            WriteFile(ConfigPath(), config::Serialize(settings));
            stamp();
        }
        if (changed) publish();

        if (now - lastReport > 30000) {
            lastReport = now;
            const auto d = camera::GetDiagnostics();
            LogF("[camera] frames=%u applied=%u rejected=%u overwritten=%u nativeDistance=%.2f",
                 d.framesSeen, d.framesApplied, d.layoutRejected, d.overwrittenByGame, d.lastNativeDistance);
        }
    }

    LogF("Unloading");
    if (dirty) WriteFile(ConfigPath(), config::Serialize(settings));
    MH_DisableHook(MH_ALL_HOOKS);
    camera::DrainNativeCamera();
    Sleep(150);   // let threads already inside the trampoline return
    MH_Uninitialize();
    FreeLibraryAndExitThread(gModule, 0);
    return 0;
}
}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        gModule = instance;
        DisableThreadLibraryCalls(instance);
        HANDLE thread = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
