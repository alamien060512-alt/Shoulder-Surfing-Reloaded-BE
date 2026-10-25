#ifndef SHOULDER_CLIENT_MODULES_CAMERA_CAMERA_MATH_H
#define SHOULDER_CLIENT_MODULES_CAMERA_CAMERA_MATH_H

#include <array>

// Pure math for the over-the-shoulder camera. No Windows or game types here so
// the whole module can be unit tested on any host.
//
// Matrices are 4x4, column-major (element(row, col) == m[col * 4 + row]) and
// are applied to column vectors, matching the render camera layout read by the
// native hook. Camera space looks down -Z: +X right, +Y up, +Z backwards.

namespace shoulder::camera {

using Mat4 = std::array<float, 16>;

struct Vec3 { float x = 0, y = 0, z = 0; };

struct ShoulderParams {
    float side = 0.75f;          // camera-space +X shift in blocks (negative = left shoulder)
    float up = 0.15f;            // camera-space +Y shift
    float back = 0.5f;           // extra distance behind the vanilla third-person camera
    float focusDistance = 24.0f; // crosshair converges on this point ahead of the head
};

struct ShoulderResult {
    Mat4 view;        // replacement world-to-camera matrix
    Mat4 inverse;     // replacement camera-to-world matrix
    Vec3 eye;         // camera world position
    float yaw = 0;    // convergence turn (radians, +left)
    float pitch = 0;  // convergence turn (radians, +up)
};

Mat4 Identity();
Mat4 Multiply(const Mat4& a, const Mat4& b);
Mat4 Translation(float x, float y, float z);
Mat4 RotationY(float radians);
Mat4 RotationX(float radians);
Mat4 Transpose3x3(const Mat4& m);   // inverse of a pure rotation
Vec3 TransformPoint(const Mat4& m, Vec3 p);

// True when `view` is a plausible rigid world-to-camera transform and
// `inverse` really inverts it (within `tolerance`). Guards against
// layout drift after a game update: never touch memory that fails this.
bool IsRigidInversePair(const Mat4& view, const Mat4& inverse, float tolerance);

// `nativeDistance` is the distance between the vanilla third-person camera
// and the player's head; the crosshair-convergence point is placed
// `focusDistance` blocks beyond the head along the vanilla view direction.
bool ApplyShoulder(const Mat4& view, const Mat4& inverse, float nativeDistance,
                   const ShoulderParams& params, ShoulderResult& out);

// Frame-rate independent exponential approach toward `target`.
float SmoothTowards(float current, float target, float seconds, float rate);

// Shrinks the offsets when the vanilla camera has been pushed in by a wall:
// without world access this is the only collision protection we have.
float CollisionScale(float nativeDistance, float nominalDistance);

}  // namespace shoulder::camera

#endif
