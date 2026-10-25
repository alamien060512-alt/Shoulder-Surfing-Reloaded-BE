#ifndef SHOULDER_CLIENT_CONFIG_SETTINGS_H
#define SHOULDER_CLIENT_CONFIG_SETTINGS_H

#include "client/modules/camera/CameraMath.h"

#include <string>

namespace shoulder::config {

struct Settings {
    bool enabled = true;
    bool leftShoulder = false;
    float sideOffset = 0.75f;        // 0..3 blocks
    float heightOffset = 0.15f;      // -1..2
    float extraDistance = 0.5f;      // 0..4
    float focusDistance = 24.0f;     // 4..128, crosshair convergence
    float nominalDistance = 4.0f;    // vanilla third-person distance (collision heuristic)
    float transitionSpeed = 12.0f;   // 1..40, smoothing rate
    bool onlyThirdPersonBack = true;
    int toggleKey = 0x75;            // F6
    int swapKey = 0x76;              // F7
    int unloadKey = 0x78;            // F9
};

// Parses "key=value" lines. Malformed values keep their defaults; numbers are clamped.
Settings Parse(const std::string& text);
std::string Serialize(const Settings& settings);

shoulder::camera::ShoulderParams ToParams(const Settings& settings, float collisionScale);

}  // namespace shoulder::config

#endif
