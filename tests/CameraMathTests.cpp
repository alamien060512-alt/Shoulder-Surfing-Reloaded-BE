#include "client/modules/camera/CameraMath.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace shoulder::camera;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static bool Near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) <= e; }

// A vanilla-like third-person camera: at world `cam`, yawed and pitched.
static void MakeCamera(Vec3 cam, float yaw, float pitch, Mat4& view, Mat4& inverse) {
    const Mat4 rotation = Multiply(RotationY(yaw), RotationX(pitch));
    inverse = Multiply(Translation(cam.x, cam.y, cam.z), rotation);
    view = Multiply(Transpose3x3(rotation), Translation(-cam.x, -cam.y, -cam.z));
}

int main() {
    Mat4 view, inverse;
    MakeCamera({100.5f, 70.0f, -230.25f}, 0.7f, -0.3f, view, inverse);
    CHECK(IsRigidInversePair(view, inverse, 0.02f));

    // Reject a broken layout (inverse of a different camera).
    Mat4 otherView, otherInverse;
    MakeCamera({0, 0, 0}, 2.0f, 0.1f, otherView, otherInverse);
    CHECK(!IsRigidInversePair(view, otherInverse, 0.02f));
    Mat4 bad = view; bad[5] = std::nanf("");
    CHECK(!IsRigidInversePair(bad, inverse, 0.02f));

    // Vanilla head is 4 blocks in front of the camera.
    const float nativeDistance = 4.0f;
    const Vec3 head = TransformPoint(inverse, {0, 0, -nativeDistance});
    const Vec3 focusWorld = TransformPoint(inverse, {0, 0, -(nativeDistance + 24.0f)});

    ShoulderParams params;
    ShoulderResult r;
    CHECK(ApplyShoulder(view, inverse, nativeDistance, params, r));
    CHECK(IsRigidInversePair(r.view, r.inverse, 0.02f));

    // Eye = vanilla camera displaced by the requested camera-space offset.
    const Vec3 expectedEye = TransformPoint(inverse, {params.side, params.up, params.back});
    CHECK(Near(r.eye.x, expectedEye.x) && Near(r.eye.y, expectedEye.y) && Near(r.eye.z, expectedEye.z));
    const Vec3 inverseOrigin = TransformPoint(r.inverse, {0, 0, 0});
    CHECK(Near(inverseOrigin.x, expectedEye.x) && Near(inverseOrigin.y, expectedEye.y) && Near(inverseOrigin.z, expectedEye.z));

    // The crosshair target (24 blocks beyond the head) lands on the view axis.
    const Vec3 focusView = TransformPoint(r.view, focusWorld);
    CHECK(Near(focusView.x, 0, 0.02f) && Near(focusView.y, 0, 0.02f) && focusView.z < 0);

    // The head stays in front of the camera and is shifted toward the opposite
    // side of the screen, which is the over-the-shoulder look.
    const Vec3 headView = TransformPoint(r.view, head);
    CHECK(headView.z < 0 && headView.x < 0);

    // Left shoulder mirrors it.
    params.side = -0.75f;
    CHECK(ApplyShoulder(view, inverse, nativeDistance, params, r));
    CHECK(TransformPoint(r.view, head).x > 0);

    // Zero offset is the identity transform.
    params = {0, 0, 0, 24.0f};
    CHECK(ApplyShoulder(view, inverse, nativeDistance, params, r));
    for (int i = 0; i < 16; ++i) CHECK(Near(r.view[i], view[i], 5e-3f));

    CHECK(!ApplyShoulder(view, inverse, -1.0f, params, r));
    CHECK(!ApplyShoulder(view, inverse, 1000.0f, params, r));

    // Smoothing converges and is monotonic; huge dt is clamped.
    float v = 0;
    for (int i = 0; i < 120; ++i) v = SmoothTowards(v, 1.0f, 1.0f / 60.0f, 12.0f);
    CHECK(v > 0.99f && v <= 1.0f);
    CHECK(SmoothTowards(0, 1, 100.0f, 12.0f) < 1.0f);
    CHECK(SmoothTowards(0, 1, 0.0f, 12.0f) == 0.0f);

    CHECK(Near(CollisionScale(4.0f, 4.0f), 1.0f));
    CHECK(Near(CollisionScale(2.0f, 4.0f), 0.5f));
    CHECK(Near(CollisionScale(9.0f, 4.0f), 1.0f));
    CHECK(Near(CollisionScale(0.0f, 4.0f), 0.0f));

    if (failures == 0) std::printf("CameraMathTests: all passed\n");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
