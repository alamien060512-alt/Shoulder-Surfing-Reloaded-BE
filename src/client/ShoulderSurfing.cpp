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
#include <fstream>
#include <sstream>
#include <string>

namespace {
HMODULE gModule = nullptr;
std::atomic<bool> gUnloadRequested{false};

std::string ReadFile(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void WriteFile(const char* path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
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
    LogF("Shoulder Surfing Reloaded (BE) starting");

    if (MH_Initialize() != MH_OK) {
        LogF("MinHook init failed");
        FreeLibraryAndExitThread(gModule, 1);
    }
    if (!camera::CreateNativeCameraHook()) {
        MH_Uninitialize();
        FreeLibraryAndExitThread(gModule, 2);
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        LogF("MH_EnableHook failed");
        camera::DrainNativeCamera();
        MH_Uninitialize();
        FreeLibraryAndExitThread(gModule, 3);
    }

    const std::string existing = ReadFile(config::kConfigPath);
    config::Settings settings = config::Parse(existing);
    if (existing.empty()) WriteFile(config::kConfigPath, config::Serialize(settings));
    FILETIME lastWrite{};
    auto stamp = [&] {
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (GetFileAttributesExA(config::kConfigPath, GetFileExInfoStandard, &data)) lastWrite = data.ftLastWriteTime;
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
    LogF("Ready. F6 toggle, F7 swap shoulder, F9 unload (see %s)", config::kConfigPath);

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
            if (GetFileAttributesExA(config::kConfigPath, GetFileExInfoStandard, &data) &&
                CompareFileTime(&data.ftLastWriteTime, &lastWrite) != 0) {
                lastWrite = data.ftLastWriteTime;
                settings = config::Parse(ReadFile(config::kConfigPath));
                changed = true;
                LogF("config reloaded");
            }
        }
        if (dirty && now - lastSave > 1000) {   // persist hotkey changes, debounced
            lastSave = now;
            dirty = false;
            WriteFile(config::kConfigPath, config::Serialize(settings));
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
    if (dirty) WriteFile(config::kConfigPath, config::Serialize(settings));
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
