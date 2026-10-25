#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "client/modules/camera/ViewSettings.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace shoulder::camera {
namespace {
std::filesystem::path EnvironmentPath(const wchar_t* name) {
    wchar_t value[32768]{};
    const DWORD size = GetEnvironmentVariableW(name, value, 32768);
    return size > 0 && size < 32768 ? std::filesystem::path(value) : std::filesystem::path{};
}
}  // namespace

int ReadGamePerspective() {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;
    const auto roaming = EnvironmentPath(L"APPDATA");
    const auto local = EnvironmentPath(L"LOCALAPPDATA");
    if (!roaming.empty()) {
        std::error_code ec;
        fs::directory_iterator it(roaming / L"Minecraft Bedrock" / L"Users", ec), end;
        for (; !ec && it != end; it.increment(ec))
            candidates.push_back(it->path() / L"games/com.mojang/minecraftpe/options.txt");
    }
    if (!local.empty())
        candidates.push_back(local / L"Packages/Microsoft.MinecraftUWP_8wekyb3d8bbwe/LocalState/games/com.mojang/minecraftpe/options.txt");

    fs::path selected;
    auto latest = fs::file_time_type::min();
    for (const auto& path : candidates) {
        std::error_code ec;
        const auto time = fs::last_write_time(path, ec);
        if (!ec && time > latest) { latest = time; selected = path; }
    }
    if (selected.empty()) return -1;

    std::ifstream stream(selected);
    std::string line;
    for (int n = 0; n < 4096 && std::getline(stream, line); ++n) {
        if (line.rfind("game_thirdperson:", 0) != 0) continue;
        char* end = nullptr;
        const long v = std::strtol(line.c_str() + 17, &end, 10);
        if (end != line.c_str() + 17 && v >= 0 && v <= 2) return static_cast<int>(v);
    }
    return -1;
}

}  // namespace shoulder::camera
