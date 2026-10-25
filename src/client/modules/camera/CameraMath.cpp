#include "client/modules/camera/CameraMath.h"

#include <algorithm>
#include <cmath>

namespace shoulder::camera {

Mat4 Identity() {
    Mat4 m{};
    m[0] = m[5] = m[10] = m[15] = 1.0f;
    return m;
}

Mat4 Multiply(const Mat4& a, const Mat4& b) {
    Mat4 out{};
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            float sum = 0;
            for (int k = 0; k < 4; ++k) sum += a[k * 4 + r] * b[c * 4 + k];
            out[c * 4 + r] = sum;
        }
    return out;
}

Mat4 Translation(float x, float y, float z) {
    Mat4 m = Identity();
    m[12] = x; m[13] = y; m[14] = z;
    return m;
}

// x' = cx + sz, z' = -sx + cz. Positive angles turn a -Z facing camera left.
Mat4 RotationY(float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    Mat4 m = Identity();
    m[0] = c;  m[8] = s;
    m[2] = -s; m[10] = c;
    return m;
}

// y' = cy - sz, z' = sy + cz. Positive angles tilt a -Z facing camera up.
Mat4 RotationX(float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    Mat4 m = Identity();
    m[5] = c;  m[9] = -s;
    m[6] = s;  m[10] = c;
    return m;
}

Mat4 Transpose3x3(const Mat4& m) {
    Mat4 out = Identity();
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) out[c * 4 + r] = m[r * 4 + c];
    return out;
}

Vec3 TransformPoint(const Mat4& m, Vec3 p) {
    return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
            m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
            m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
}

bool IsRigidInversePair(const Mat4& view, const Mat4& inverse, float tolerance) {
    for (float v : view) if (!std::isfinite(v)) return false;
    for (float v : inverse) if (!std::isfinite(v)) return false;
    // Bottom row of an affine transform.
    for (const Mat4* m : {&view, &inverse}) {
        if (std::fabs((*m)[3]) > 1e-3f || std::fabs((*m)[7]) > 1e-3f ||
            std::fabs((*m)[11]) > 1e-3f || std::fabs((*m)[15] - 1.0f) > 1e-3f) return false;
    }
    // Orthonormal 3x3 rotation block (columns unit length and mutually orthogonal).
    for (int a = 0; a < 3; ++a)
        for (int b = a; b < 3; ++b) {
            float dot = 0;
            for (int r = 0; r < 3; ++r) dot += view[a * 4 + r] * view[b * 4 + r];
            if (std::fabs(dot - (a == b ? 1.0f : 0.0f)) > 0.01f) return false;
        }
    // The two matrices must invert each other. Translation terms lose precision
    // at large world coordinates, so only the rotation block uses the strict bound.
    const Mat4 product = Multiply(view, inverse);
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            const float expected = r == c ? 1.0f : 0.0f;
            const float allowed = c == 3 && r < 3 ? std::max(tolerance, 4.0f) : tolerance;
            if (!std::isfinite(product[c * 4 + r]) || std::fabs(product[c * 4 + r] - expected) > allowed) return false;
        }
    return true;
}

bool ApplyShoulder(const Mat4& view, const Mat4& inverse, float nativeDistance,
                   const ShoulderParams& p, ShoulderResult& out) {
    if (!std::isfinite(nativeDistance) || nativeDistance < 0 || nativeDistance > 64) return false;
    const Vec3 t{p.side, p.up, p.back};
    // Vanilla head sits nativeDistance in front of the vanilla camera. The new
    // camera aims at the point focusDistance beyond it so the crosshair keeps
    // pointing where the player's own aim ray lands.
    const Vec3 focus{0, 0, -(nativeDistance + std::max(p.focusDistance, 1.0f))};
    const Vec3 d{focus.x - t.x, focus.y - t.y, focus.z - t.z};
    out.yaw = std::atan2(-d.x, -d.z);
    out.pitch = std::atan2(d.y, std::hypot(d.x, d.z));
    const Mat4 rotation = Multiply(RotationY(out.yaw), RotationX(out.pitch));
    out.view = Multiply(Transpose3x3(rotation), Multiply(Translation(-t.x, -t.y, -t.z), view));
    out.inverse = Multiply(inverse, Multiply(Translation(t.x, t.y, t.z), rotation));
    out.eye = TransformPoint(inverse, t);
    return true;
}

float SmoothTowards(float current, float target, float seconds, float rate) {
    if (!std::isfinite(current) || !std::isfinite(target)) return target;
    if (!(seconds > 0) || !(rate > 0)) return current;
    const float k = 1.0f - std::exp(-rate * std::min(seconds, 0.25f));
    return current + (target - current) * k;
}

float CollisionScale(float nativeDistance, float nominalDistance) {
    if (!(nominalDistance > 0.1f) || !std::isfinite(nativeDistance)) return 1.0f;
    return std::clamp(nativeDistance / nominalDistance, 0.0f, 1.0f);
}

}  // namespace shoulder::camera
