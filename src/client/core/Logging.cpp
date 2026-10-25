#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "client/core/Logging.h"

#include <cstdarg>
#include <cstdio>

namespace shoulder {

void LogF(const char* format, ...) {
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    OutputDebugStringA("[ShoulderSurfing] ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");

    SYSTEMTIME time;
    GetLocalTime(&time);
    char line[1200];
    const int length = snprintf(line, sizeof(line), "%02d:%02d:%02d  %s\r\n",
                                time.wHour, time.wMinute, time.wSecond, message);
    if (length <= 0) return;

    static wchar_t path[MAX_PATH] = {};
    if (!path[0]) {
        wchar_t temp[MAX_PATH] = {};
        const DWORD n = GetTempPathW(MAX_PATH, temp);
        if (n == 0 || n >= MAX_PATH - 40) return;
        wcscpy_s(path, temp);
        wcscat_s(path, L"shoulder-surfing-reloaded.log");
    }

    HANDLE file = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 5; ++attempt) {
        file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) break;
        Sleep(1);
    }
    if (file == INVALID_HANDLE_VALUE) return;

    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(length), &written, nullptr);
    CloseHandle(file);
}

}  // namespace shoulder
