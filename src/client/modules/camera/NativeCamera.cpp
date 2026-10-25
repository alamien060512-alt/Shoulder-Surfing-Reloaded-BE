#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <MinHook.h>

#include "client/modules/camera/NativeCamera.h"
#include "client/core/Logging.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>

namespace shoulder::camera {
namespace {
using Address = std::uintptr_t;
using Render = void(__fastcall*)(void*, void*, const void*, void*);

Render original = nullptr;
std::atomic<bool> available{false}, stopping{false};
std::atomic<unsigned> inFlight{0};

std::mutex controlsMutex;
Controls controls;
std::mutex diagMutex;
Diagnostics diag;

// Minecraft 1.26.52.3 profile, taken from the LevelRendererCamera::render
// lambda (same entry point the Midnight capture hook uses). Everything below
// is validated against the live process before a single byte is written.
constexpr Address kRenderRva = 0x4686F20;
constexpr Address kRenderVirtualSlotRva = 0xE8DE058;
constexpr Address kCameraOffset = 0x740;     // renderer + this = camera block
// Camera block: +0x000 view matrix stack (std::deque<Mat4>-like header),
//               +0x080 projection stack, +0x0C0 inverse view, +0x124 world eye.
constexpr Address kInverseOffset = 0xC0;
constexpr Address kEyeOffset = 0x124;
constexpr Address kShadowFlagOffset = 134;   // ViewRenderData::mIsShadowPass

bool Copy(Address address, void* out, std::size_t size) {
    if (address < 0x10000 || !out || size > 0x10000 || address > UINTPTR_MAX - size) return false;
    SIZE_T done = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), out, size, &done) && done == size;
}
bool Store(Address address, const void* data, std::size_t size) {
    if (address < 0x10000 || !data || size > 0x1000 || address > UINTPTR_MAX - size) return false;
    SIZE_T done = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), data, size, &done) && done == size;
}
template <class T> bool Read(Address address, T& value) { return Copy(address, &value, sizeof(value)); }

// Address of the top 4x4 matrix of a deque-of-matrices (one matrix per block).
bool MatrixTopAddress(Address stack, Address& out) {
    std::uint64_t f[5]{};
    if (!Read(stack, f)) return false;
    const auto count = f[2], size = f[4];
    if (!f[1] || count < 8 || count > 4096 || (count & (count - 1)) ||
        size == 0 || size > count || f[3] > UINT64_MAX - size) return false;
    Address block = 0;
    if (!Read(f[1] + ((f[3] + size - 1) & (count - 1)) * 8, block) || block < 0x10000) return false;
    out = block;
    return true;
}

bool FinitePosition(const Vec3& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
           std::fabs(p.x) < 35e6f && std::fabs(p.y) < 35e6f && std::fabs(p.z) < 35e6f;
}

struct Animation {
    float side = 0, up = 0, back = 0;
    LARGE_INTEGER last{};
    bool primed = false;
} anim;   // render thread only

float FrameSeconds() {
    static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float seconds = 1.0f / 60.0f;
    if (anim.primed && freq.QuadPart > 0) seconds = float(double(now.QuadPart - anim.last.QuadPart) / double(freq.QuadPart));
    anim.last = now;
    anim.primed = true;
    return std::clamp(seconds, 0.0f, 0.25f);
}

void OnRender(void* renderer, void* context, const void* viewObject, void* client) {
    struct Guard { Guard() { ++inFlight; } ~Guard() { --inFlight; } } guard;
    if (stopping.load(std::memory_order_acquire) || !renderer || !viewObject) {
        original(renderer, context, viewObject, client);
        return;
    }

    const Address cameraBlock = reinterpret_cast<Address>(renderer) + kCameraOffset;
    const Address view = reinterpret_cast<Address>(viewObject);

    std::uint8_t shadowPass = 1;
    if (!Read(view + kShadowFlagOffset, shadowPass) || shadowPass) {   // shadow/reflection cameras
        original(renderer, context, viewObject, client);
        return;
    }

    Controls c;
    { std::lock_guard<std::mutex> lock(controlsMutex); c = controls; }
    const float dt = FrameSeconds();

    Vec3 headEye{}, cameraEye{};
    float nativeDistance = 0;
    bool thirdPerson = false;
    if (Read(view, headEye) && Read(cameraBlock + kEyeOffset, cameraEye) &&
        FinitePosition(headEye) && FinitePosition(cameraEye)) {
        const float dx = headEye.x - cameraEye.x, dy = headEye.y - cameraEye.y, dz = headEye.z - cameraEye.z;
        nativeDistance = std::sqrt(dx * dx + dy * dy + dz * dz);
        // First person has the camera at the head; any real gap means a chase camera.
        thirdPerson = nativeDistance > 0.5f && nativeDistance < 16.0f;
    }

    const bool wanted = c.active && thirdPerson && !(c.excludeFrontView && c.optionsPerspective == 2);
    const float scale = wanted ? CollisionScale(nativeDistance, c.nominalDistance) : 1.0f;
    const float rate = std::max(c.transitionSpeed, 1.0f);
    anim.side = SmoothTowards(anim.side, wanted ? c.target.side * scale : 0.0f, dt, rate);
    anim.up = SmoothTowards(anim.up, wanted ? c.target.up * scale : 0.0f, dt, rate);
    anim.back = SmoothTowards(anim.back, wanted ? c.target.back * scale : 0.0f, dt, rate);

    {
        std::lock_guard<std::mutex> lock(diagMutex);
        ++diag.framesSeen;
        diag.lastNativeDistance = nativeDistance;
    }

    if (!thirdPerson || (std::fabs(anim.side) < 1e-3f && std::fabs(anim.up) < 1e-3f && std::fabs(anim.back) < 1e-3f)) {
        original(renderer, context, viewObject, client);
        return;
    }

    Address matrixAddress = 0;
    Mat4 viewMatrix{}, inverseMatrix{};
    ShoulderResult result;
    ShoulderParams params = c.target;
    params.side = anim.side; params.up = anim.up; params.back = anim.back;
    if (!MatrixTopAddress(cameraBlock, matrixAddress) || !Copy(matrixAddress, viewMatrix.data(), 64) ||
        !Copy(cameraBlock + kInverseOffset, inverseMatrix.data(), 64) ||
        !IsRigidInversePair(viewMatrix, inverseMatrix, 0.02f) ||
        !ApplyShoulder(viewMatrix, inverseMatrix, nativeDistance, params, result)) {
        std::lock_guard<std::mutex> lock(diagMutex);
        ++diag.layoutRejected;
        original(renderer, context, viewObject, client);
        return;
    }

    // Write the shifted camera for this render call only and put the vanilla
    // one back afterwards so gameplay code never sees our offset.
    const Vec3 shiftedEye{result.eye.x, result.eye.y, result.eye.z};
    const bool wrote = Store(matrixAddress, result.view.data(), 64) &&
                       Store(cameraBlock + kInverseOffset, result.inverse.data(), 64) &&
                       Store(cameraBlock + kEyeOffset, &shiftedEye, sizeof(shiftedEye));
    original(renderer, context, viewObject, client);

    if (wrote) {
        Mat4 after{};
        const bool overwritten = Copy(matrixAddress, after.data(), 64) &&
                                 std::memcmp(after.data(), result.view.data(), 64) != 0;
        Store(matrixAddress, viewMatrix.data(), 64);
        Store(cameraBlock + kInverseOffset, inverseMatrix.data(), 64);
        Store(cameraBlock + kEyeOffset, &cameraEye, sizeof(cameraEye));
        std::lock_guard<std::mutex> lock(diagMutex);
        ++diag.framesApplied;
        if (overwritten) ++diag.overwrittenByGame;
    }
}

bool BytesMatch(Address at, const unsigned char* expected, std::size_t length) {
    unsigned char actual[64]{};
    return length <= sizeof(actual) && Copy(at, actual, length) && !std::memcmp(actual, expected, length);
}
}  // namespace

bool CreateNativeCameraHook() {
    const Address base = reinterpret_cast<Address>(GetModuleHandleW(nullptr));
    constexpr unsigned char prologue[] = {0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x56, 0x57, 0x53,
        0x48, 0x81, 0xEC, 0x98, 0x0C, 0, 0, 0x48, 0x8D, 0xAC, 0x24, 0x80, 0, 0, 0, 0x44, 0x0F, 0x29, 0xA5, 0};
    Address slot = 0;
    if (!base || !BytesMatch(base + kRenderRva, prologue, sizeof(prologue)) ||
        !Read(base + kRenderVirtualSlotRva, slot) || slot != base + kRenderRva) {
        LogF("[camera] binary profile mismatch (expected Minecraft 1.26.52.3); hook not installed");
        return false;
    }
    const auto status = MH_CreateHook(reinterpret_cast<void*>(base + kRenderRva),
                                      reinterpret_cast<void*>(&OnRender), reinterpret_cast<void**>(&original));
    available.store(status == MH_OK, std::memory_order_release);
    LogF("[camera] world-render hook create=%d", static_cast<int>(status));
    return status == MH_OK;
}

bool IsNativeCameraHookAvailable() { return available.load(std::memory_order_acquire); }

void PublishControls(const Controls& value) {
    std::lock_guard<std::mutex> lock(controlsMutex);
    controls = value;
}

Diagnostics GetDiagnostics() {
    std::lock_guard<std::mutex> lock(diagMutex);
    return diag;
}

void DrainNativeCamera() {
    stopping.store(true, std::memory_order_release);
    while (inFlight.load(std::memory_order_acquire)) Sleep(1);
}

}  // namespace shoulder::camera
