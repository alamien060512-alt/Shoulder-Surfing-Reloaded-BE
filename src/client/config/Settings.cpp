#include "client/config/Settings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace shoulder::config {
namespace {

std::string Trim(std::string s) {
    const char* ws = " \t\r\n";
    const auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

bool ParseFloat(const std::string& v, float lo, float hi, float& out) {
    char* end = nullptr;
    const float parsed = std::strtof(v.c_str(), &end);
    if (end == v.c_str() || *end != '\0' || !std::isfinite(parsed)) return false;
    out = std::clamp(parsed, lo, hi);
    return true;
}

bool ParseBool(const std::string& v, bool& out) {
    if (v == "1" || v == "true" || v == "on" || v == "yes") { out = true; return true; }
    if (v == "0" || v == "false" || v == "off" || v == "no") { out = false; return true; }
    return false;
}

bool ParseKey(const std::string& v, int& out) {
    char* end = nullptr;
    const long parsed = std::strtol(v.c_str(), &end, 0);   // decimal or 0x..
    if (end == v.c_str() || *end != '\0' || parsed < 0 || parsed > 0xFE) return false;
    out = static_cast<int>(parsed);
    return true;
}

}  // namespace

Settings Parse(const std::string& text) {
    Settings s;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq)), value = Trim(line.substr(eq + 1));
        if (key == "enabled") ParseBool(value, s.enabled);
        else if (key == "left_shoulder") ParseBool(value, s.leftShoulder);
        else if (key == "side_offset") ParseFloat(value, 0.0f, 3.0f, s.sideOffset);
        else if (key == "height_offset") ParseFloat(value, -1.0f, 2.0f, s.heightOffset);
        else if (key == "extra_distance") ParseFloat(value, 0.0f, 4.0f, s.extraDistance);
        else if (key == "focus_distance") ParseFloat(value, 4.0f, 128.0f, s.focusDistance);
        else if (key == "native_distance") ParseFloat(value, 1.0f, 16.0f, s.nominalDistance);
        else if (key == "transition_speed") ParseFloat(value, 1.0f, 40.0f, s.transitionSpeed);
        else if (key == "only_third_person_back") ParseBool(value, s.onlyThirdPersonBack);
        else if (key == "toggle_key") ParseKey(value, s.toggleKey);
        else if (key == "swap_key") ParseKey(value, s.swapKey);
        else if (key == "unload_key") ParseKey(value, s.unloadKey);
    }
    return s;
}

std::string Serialize(const Settings& s) {
    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "# Shoulder Surfing Reloaded (Bedrock) - edit while the game runs; reloads automatically.\n"
        "enabled=%d\n"
        "left_shoulder=%d\n"
        "# Camera shift in blocks.\n"
        "side_offset=%.2f\n"
        "height_offset=%.2f\n"
        "extra_distance=%.2f\n"
        "# Crosshair converges on a point this many blocks ahead of the player.\n"
        "focus_distance=%.1f\n"
        "# Vanilla third-person distance; offsets shrink when a wall pushes the camera closer.\n"
        "native_distance=%.1f\n"
        "transition_speed=%.1f\n"
        "only_third_person_back=%d\n"
        "# Windows virtual-key codes: F6=0x75, F7=0x76, F9=0x78\n"
        "toggle_key=0x%02X\n"
        "swap_key=0x%02X\n"
        "unload_key=0x%02X\n",
        s.enabled, s.leftShoulder, s.sideOffset, s.heightOffset, s.extraDistance,
        s.focusDistance, s.nominalDistance, s.transitionSpeed, s.onlyThirdPersonBack,
        s.toggleKey, s.swapKey, s.unloadKey);
    return buf;
}

shoulder::camera::ShoulderParams ToParams(const Settings& s, float collisionScale) {
    shoulder::camera::ShoulderParams p;
    const float scale = std::clamp(collisionScale, 0.0f, 1.0f);
    p.side = (s.leftShoulder ? -s.sideOffset : s.sideOffset) * scale;
    p.up = s.heightOffset * scale;
    p.back = s.extraDistance * scale;
    p.focusDistance = s.focusDistance;
    return p;
}

}  // namespace shoulder::config
