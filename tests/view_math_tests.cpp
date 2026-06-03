// Characterization tests for the lookAt-basis head-tracking math in
// src/view_math.h. These lock the orientation transforms applied to the
// engine's focus/up vectors: a transcription error (or any future "cleanup"
// of the arithmetic) changes where the camera looks in a way no compiler
// error would catch.

#include "view_math.h"

#include <cmath>
#include <iostream>

namespace {

using cameraunlock::math::Vec3;
using yakuza0::ApplyHeadPose;
using yakuza0::BuildCameraBasis;
using yakuza0::CameraBasis;
using yakuza0::TrackedOrientation;

int g_failures = 0;

void Check(bool cond, const char* name) {
    if (cond) {
        std::cout << "  [PASS] " << name << "\n";
    } else {
        std::cout << "  [FAIL] " << name << "\n";
        ++g_failures;
    }
}

bool NearEqual(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

bool VecNear(const Vec3& v, float x, float y, float z, float eps = 1e-4f) {
    return NearEqual(v.x, x, eps) && NearEqual(v.y, y, eps) && NearEqual(v.z, z, eps);
}

bool VecNear(const Vec3& a, const Vec3& b, float eps = 1e-4f) {
    return VecNear(a, b.x, b.y, b.z, eps);
}

// Level camera at origin looking down +Z, 5 units of focal distance.
CameraBasis LevelBasis() {
    CameraBasis basis;
    bool ok = BuildCameraBasis(Vec3(0, 0, 0), Vec3(0, 0, 5), Vec3(0, 1, 0), basis);
    Check(ok, "level camera produces a valid basis");
    return basis;
}

const float kInvSqrt2 = 0.70710678f;

}  // namespace

int RunViewMathTests() {
    std::cout << "View basis math tests\n";

    // Basis construction.
    {
        CameraBasis basis = LevelBasis();
        Check(VecNear(basis.forward, 0, 0, 1), "level basis forward is +Z");
        Check(VecNear(basis.up, 0, 1, 0), "level basis up is +Y");
        Check(VecNear(basis.right, -1, 0, 0), "level basis right is -X (engine handedness)");
        Check(NearEqual(basis.focalDistance, 5.0f), "focal distance preserved");
    }

    // Degenerate inputs refuse to build a basis.
    {
        CameraBasis basis;
        Check(!BuildCameraBasis(Vec3(1, 2, 3), Vec3(1, 2, 3), Vec3(0, 1, 0), basis),
              "focus on top of position is rejected");
        Check(!BuildCameraBasis(Vec3(0, 0, 0), Vec3(0, 0, 5), Vec3(0, 0, 1), basis),
              "up parallel to forward is rejected");
    }

    // Identity pose is a no-op in both yaw modes.
    {
        CameraBasis basis = LevelBasis();
        for (bool worldYaw : {false, true}) {
            TrackedOrientation o = ApplyHeadPose(basis, 0, 0, 0, worldYaw);
            Check(VecNear(o.forward, basis.forward) && VecNear(o.up, basis.up),
                  worldYaw ? "identity pose is a no-op (world yaw)"
                           : "identity pose is a no-op (camera-local yaw)");
        }
    }

    // Pure yaw on a level camera turns toward the basis right vector.
    {
        CameraBasis basis = LevelBasis();
        TrackedOrientation o = ApplyHeadPose(basis, 90.0f, 0, 0, false);
        Check(VecNear(o.forward, -1, 0, 0), "+90 yaw forward lands on basis right (-X)");
        Check(VecNear(o.up, 0, 1, 0), "+90 yaw leaves up untouched");
    }

    // Pure pitch on a level camera: +pitch looks down in the engine basis
    // (OpenTrack pitch-up is inverted upstream via SensitivitySettings).
    {
        CameraBasis basis = LevelBasis();
        TrackedOrientation o = ApplyHeadPose(basis, 0, 90.0f, 0, false);
        Check(VecNear(o.forward, 0, -1, 0), "+90 pitch forward points straight down");
        Check(VecNear(o.up, 0, 0, 1), "+90 pitch up points along old forward");
    }

    // The two yaw modes agree exactly while the camera is level.
    {
        CameraBasis basis = LevelBasis();
        TrackedOrientation local = ApplyHeadPose(basis, 30.0f, 20.0f, 10.0f, false);
        TrackedOrientation world = ApplyHeadPose(basis, 30.0f, 20.0f, 10.0f, true);
        Check(VecNear(local.forward, world.forward, 1e-3f) && VecNear(local.up, world.up, 1e-3f),
              "world and camera-local yaw agree for a level camera");
    }

    // Pitched-down camera: world yaw is horizon-locked (keeps the downward
    // pitch while turning), camera-local yaw orbits the tilted up-axis.
    {
        CameraBasis basis;
        bool ok = BuildCameraBasis(Vec3(0, 0, 0), Vec3(0, -5, 5),
                                   Vec3(0, kInvSqrt2, kInvSqrt2), basis);
        Check(ok, "45-degree pitched camera produces a valid basis");

        TrackedOrientation world = ApplyHeadPose(basis, 90.0f, 0, 0, true);
        Check(VecNear(world.forward, -kInvSqrt2, -kInvSqrt2, 0, 1e-3f),
              "world yaw preserves downward pitch while turning 90deg");

        TrackedOrientation local = ApplyHeadPose(basis, 90.0f, 0, 0, false);
        Check(VecNear(local.forward, -1, 0, 0, 1e-3f),
              "camera-local yaw orbits the tilted up-axis instead");
    }

    // Camera looking straight down: world yaw falls back to camera-local.
    {
        CameraBasis basis;
        bool ok = BuildCameraBasis(Vec3(0, 0, 0), Vec3(0, -5, 0), Vec3(0, 0, 1), basis);
        Check(ok, "straight-down camera produces a valid basis");

        TrackedOrientation world = ApplyHeadPose(basis, 35.0f, 10.0f, 5.0f, true);
        TrackedOrientation local = ApplyHeadPose(basis, 35.0f, 10.0f, 5.0f, false);
        Check(VecNear(world.forward, local.forward) && VecNear(world.up, local.up),
              "straight-down world yaw falls back to camera-local");
    }

    // Orientation output stays orthonormal for arbitrary poses.
    {
        CameraBasis basis;
        BuildCameraBasis(Vec3(3, 1, -2), Vec3(7, 0, 4), Vec3(0.1f, 0.9f, 0.2f), basis);
        for (bool worldYaw : {false, true}) {
            TrackedOrientation o = ApplyHeadPose(basis, -47.0f, 23.0f, 12.0f, worldYaw);
            const bool unitLen = NearEqual(o.forward.Magnitude(), 1.0f, 1e-3f) &&
                                 NearEqual(o.up.Magnitude(), 1.0f, 1e-3f);
            const bool ortho = NearEqual(Vec3::Dot(o.forward, o.up), 0.0f, 1e-3f);
            Check(unitLen && ortho,
                  worldYaw ? "tracked orientation stays orthonormal (world yaw)"
                           : "tracked orientation stays orthonormal (camera-local yaw)");
        }
    }

    // 6DOF position offsets map through the clean basis.
    {
        CameraBasis basis = LevelBasis();
        Check(VecNear(basis.LocalToWorld(Vec3(1, 0, 0)), basis.right),
              "local +X maps to basis right");
        Check(VecNear(basis.LocalToWorld(Vec3(0, 2, 0)), Vec3(0, 2, 0)),
              "local +Y maps to basis up, scaled");
        Check(VecNear(basis.LocalToWorld(Vec3(0, 0, -3)), Vec3(0, 0, -3)),
              "local -Z maps backward along basis forward");
    }

    if (g_failures == 0) std::cout << "View math tests: all passed\n";
    return g_failures;
}
