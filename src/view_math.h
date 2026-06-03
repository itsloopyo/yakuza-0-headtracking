#pragma once

#include <cameraunlock/math/quat4.h>
#include <cameraunlock/math/vec3.h>

// Pure orientation math for the engine's lookAt (eye/focus/up) camera model.
// No engine, logging, or threading dependencies so the arithmetic is
// unit-testable; the hook path in mod.cpp is a thin caller.
namespace yakuza0 {

namespace math = ::cameraunlock::math;

// Orthonormal clean camera basis derived from the engine's lookAt state.
struct CameraBasis {
    math::Vec3 right;
    math::Vec3 up;
    math::Vec3 forward;
    float focalDistance = 0.0f;  // |focus - position|, preserved on writeback

    math::Vec3 LocalToWorld(const math::Vec3& v) const {
        return right * v.x + up * v.y + forward * v.z;
    }
};

// Builds the clean orthonormal basis from (focus - position) and up.
// Returns false when the basis is degenerate (focus on top of position, or
// up parallel to forward); injection must be skipped that frame.
inline bool BuildCameraBasis(const math::Vec3& position, const math::Vec3& focus,
                             const math::Vec3& up, CameraBasis& out) {
    using math::Vec3;

    Vec3 fwd = focus - position;
    const float fwdLen = fwd.Magnitude();
    if (fwdLen < 1e-4f) return false;
    Vec3 fwdN(fwd.x / fwdLen, fwd.y / fwdLen, fwd.z / fwdLen);

    Vec3 right = Vec3::Cross(fwdN, up).Normalized();
    if (right.SqrMagnitude() < 1e-8f) return false;

    out.right = right;
    out.up = Vec3::Cross(right, fwdN);
    out.forward = fwdN;
    out.focalDistance = fwdLen;
    return true;
}

// Head-tracked camera orientation: new forward and up unit vectors.
struct TrackedOrientation {
    math::Vec3 forward;
    math::Vec3 up;
};

// Applies the head pose (degrees) to the clean basis.
//
// Two yaw modes, identical whenever the game camera is level; they diverge
// once the camera is pitched (e.g. looking at the floor):
//   world-space - yaw rotates the clean basis around the world up-axis
//                 first, then pitch/roll apply camera-locally in the
//                 yawed basis ("up" stays a constant).
//   camera-local - all three axes composed into a single YXZ quaternion
//                 applied in the camera basis (yaw follows the camera's
//                 own up-axis, leaning at extreme pitch).
// World-space falls back to camera-local when the camera looks straight
// up/down (the horizon basis is degenerate that frame).
inline TrackedOrientation ApplyHeadPose(const CameraBasis& basis,
                                        float yawDeg, float pitchDeg, float rollDeg,
                                        bool worldSpaceYaw) {
    using math::Quat4;
    using math::Vec3;

    TrackedOrientation result;
    bool worldYaw = worldSpaceYaw;
    if (worldYaw) {
        const Vec3 worldUp(0.0f, 1.0f, 0.0f);
        Vec3 flatFwd = basis.forward - worldUp * Vec3::Dot(basis.forward, worldUp);
        if (flatFwd.SqrMagnitude() < 1e-6f) {
            worldYaw = false;
        } else {
            flatFwd = flatFwd.Normalized();
            // Same (right, up, forward) frame convention as the camera
            // basis so the yaw sign matches the camera-local branch.
            Vec3 flatRight = Vec3::Cross(flatFwd, worldUp).Normalized();

            Quat4 yawQ = Quat4::FromYawPitchRoll(yawDeg, 0.0f, 0.0f);
            auto yawAroundWorldUp = [&](const Vec3& v) {
                Vec3 local(Vec3::Dot(v, flatRight), Vec3::Dot(v, worldUp), Vec3::Dot(v, flatFwd));
                Vec3 r = yawQ.Rotate(local);
                return flatRight * r.x + worldUp * r.y + flatFwd * r.z;
            };
            Vec3 yawedRight = yawAroundWorldUp(basis.right);
            Vec3 yawedUp    = yawAroundWorldUp(basis.up);
            Vec3 yawedFwd   = yawAroundWorldUp(basis.forward);

            Quat4 prQ = Quat4::FromYawPitchRoll(0.0f, pitchDeg, rollDeg);
            Vec3 fwdLocal = prQ.Rotate(Vec3(0.0f, 0.0f, 1.0f));
            Vec3 upLocal  = prQ.Rotate(Vec3(0.0f, 1.0f, 0.0f));
            result.forward = yawedRight * fwdLocal.x + yawedUp * fwdLocal.y + yawedFwd * fwdLocal.z;
            result.up      = yawedRight * upLocal.x  + yawedUp * upLocal.y  + yawedFwd * upLocal.z;
        }
    }
    if (!worldYaw) {
        // Tracked rotation in head-local coords (X=right, Y=up, Z=forward).
        Quat4 R = Quat4::FromYawPitchRoll(yawDeg, pitchDeg, rollDeg);
        Vec3 fwdLocal = R.Rotate(Vec3(0.0f, 0.0f, 1.0f));
        Vec3 upLocal  = R.Rotate(Vec3(0.0f, 1.0f, 0.0f));
        result.forward = basis.LocalToWorld(fwdLocal);
        result.up      = basis.LocalToWorld(upLocal);
    }
    return result;
}

}  // namespace yakuza0
