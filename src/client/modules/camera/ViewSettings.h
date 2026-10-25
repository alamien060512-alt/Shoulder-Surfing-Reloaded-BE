#ifndef SHOULDER_CLIENT_MODULES_CAMERA_VIEW_SETTINGS_H
#define SHOULDER_CLIENT_MODULES_CAMERA_VIEW_SETTINGS_H

namespace shoulder::camera {

// Reads `game_thirdperson` (0 first person, 1 back, 2 front) from the newest
// Minecraft options.txt. File I/O: call from the worker thread only.
// Returns -1 when no options file could be read.
int ReadGamePerspective();

}  // namespace shoulder::camera

#endif
