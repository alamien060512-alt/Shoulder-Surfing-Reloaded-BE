#ifndef SHOULDER_CLIENT_MODULES_CAMERA_NATIVE_CAMERA_H
#define SHOULDER_CLIENT_MODULES_CAMERA_NATIVE_CAMERA_H

#include "client/modules/camera/CameraMath.h"

#include <cstdint>

namespace shoulder::camera {

// Published by the worker thread, consumed by the render hook once per frame.
struct Controls {
    bool active = false;                 // master switch
    ShoulderParams target;               // fully signed target offsets (side < 0 = left)
    float nominalDistance = 4.0f;        // vanilla third-person distance
    float transitionSpeed = 12.0f;
    bool excludeFrontView = true;        // skip the "front" perspective
    int optionsPerspective = -1;         // game_thirdperson from options.txt, -1 = unknown
};

struct Diagnostics {
    std::uint32_t framesSeen = 0;
    std::uint32_t framesApplied = 0;
    std::uint32_t layoutRejected = 0;    // matrices failed validation: nothing was written
    std::uint32_t overwrittenByGame = 0; // render call replaced our matrices
    float lastNativeDistance = 0;
};

// Validates the binary profile and creates (does not enable) the hook.
// Returns false and leaves the game untouched on any mismatch.
// `quiet` suppresses the mismatch log so callers can poll while the game boots.
bool CreateNativeCameraHook(bool quiet = false);
bool IsNativeCameraHookAvailable();

void PublishControls(const Controls& controls);
Diagnostics GetDiagnostics();

// Call after MH_DisableHook and before MH_Uninitialize.
void DrainNativeCamera();

}  // namespace shoulder::camera

#endif
