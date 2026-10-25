// Minimal command-line injector: Injector.exe [path\to\ShoulderSurfing.dll]
// Exit codes: 0 injected / already loaded, 1 DLL or game not found, 2 injection failed.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <string>

namespace {
const wchar_t* const kTargets[] = {L"Minecraft.Windows.exe", L"Minecraft.WindowsPreview.exe"};

DWORD FindGame() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W entry{sizeof(entry)};
    DWORD pid = 0;
    for (BOOL ok = Process32FirstW(snap, &entry); ok && !pid; ok = Process32NextW(snap, &entry))
        for (auto name : kTargets)
            if (!_wcsicmp(entry.szExeFile, name)) pid = entry.th32ProcessID;
    CloseHandle(snap);
    return pid;
}

bool AlreadyLoaded(DWORD pid, const wchar_t* moduleName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W m{sizeof(m)};
    bool found = false;
    for (BOOL ok = Module32FirstW(snap, &m); ok && !found; ok = Module32NextW(snap, &m))
        found = !_wcsicmp(m.szModule, moduleName);
    CloseHandle(snap);
    return found;
}

// Minecraft is a packaged (AppContainer) app: its token cannot read the DLL
// unless ALL APPLICATION PACKAGES (S-1-15-2-1) has read/execute on it.
void GrantPackageAccess(const std::wstring& dll) {
    std::wstring cmd = L"icacls \"" + dll + L"\" /grant *S-1-15-2-1:(RX)";
    STARTUPINFOW si{sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 10000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
    wchar_t self[MAX_PATH]{};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring dir(self);
    dir.erase(dir.find_last_of(L"\\/") + 1);
    std::wstring dll = argc > 1 ? argv[1] : dir + L"ShoulderSurfing.dll";

    wchar_t full[MAX_PATH]{};
    if (!GetFullPathNameW(dll.c_str(), MAX_PATH, full, nullptr) || GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) {
        std::fwprintf(stderr, L"DLL not found: %ls\n", dll.c_str());
        return 1;
    }
    const DWORD pid = FindGame();
    if (!pid) { std::fwprintf(stderr, L"Minecraft is not running.\n"); return 1; }
    if (AlreadyLoaded(pid, L"ShoulderSurfing.dll")) { std::wprintf(L"Already loaded.\n"); return 0; }

    GrantPackageAccess(full);

    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                 PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!process) { std::fwprintf(stderr, L"OpenProcess failed (%lu). Try running as administrator.\n", GetLastError()); return 2; }

    const SIZE_T bytes = (wcslen(full) + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    int code = 2;
    if (remote && WriteProcessMemory(process, remote, full, bytes, nullptr)) {
        auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
        HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibrary, remote, 0, nullptr);
        if (thread) {
            WaitForSingleObject(thread, 10000);
            DWORD result = 0;
            GetExitCodeThread(thread, &result);
            CloseHandle(thread);
            if (result || AlreadyLoaded(pid, L"ShoulderSurfing.dll")) { std::wprintf(L"Injected into PID %lu.\n", pid); code = 0; }
            else std::fwprintf(stderr, L"LoadLibrary failed inside the game (DLL unreadable or profile rejected).\n");
        } else std::fwprintf(stderr, L"CreateRemoteThread failed (%lu).\n", GetLastError());
    }
    if (remote) VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    CloseHandle(process);
    return code;
}
