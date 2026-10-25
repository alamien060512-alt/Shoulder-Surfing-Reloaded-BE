#include "client/config/Settings.h"

#include <cstdio>
#include <cstdlib>

using namespace shoulder::config;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    Settings d;
    Settings s = Parse("");
    CHECK(s.enabled == d.enabled && s.sideOffset == d.sideOffset);

    s = Parse("# c\nenabled=off\nside_offset=99\nheight_offset=-50\nfocus_distance=abc\nswap_key=0x41\n  left_shoulder = 1 \nbogus\n");
    CHECK(!s.enabled);
    CHECK(s.sideOffset == 3.0f);
    CHECK(s.heightOffset == -1.0f);
    CHECK(s.focusDistance == d.focusDistance);   // malformed keeps default
    CHECK(s.swapKey == 0x41);
    CHECK(s.leftShoulder);

    s = Parse("toggle_key=999\nnative_distance=nan\ntransition_speed=inf\n");
    CHECK(s.toggleKey == d.toggleKey);
    CHECK(s.nominalDistance == d.nominalDistance);
    CHECK(s.transitionSpeed == d.transitionSpeed);

    // Round trip.
    Settings a;
    a.leftShoulder = true; a.sideOffset = 1.25f; a.extraDistance = 2.0f; a.swapKey = 0x42;
    Settings b = Parse(Serialize(a));
    CHECK(b.leftShoulder && b.sideOffset == 1.25f && b.extraDistance == 2.0f && b.swapKey == 0x42);

    auto p = ToParams(a, 0.5f);
    CHECK(p.side == -0.625f && p.back == 1.0f);
    p = ToParams(a, 7.0f);   // clamped
    CHECK(p.side == -1.25f);

    if (failures == 0) std::printf("SettingsTests: all passed\n");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
