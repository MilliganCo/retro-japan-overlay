#include "MouseField.h"

#include <cmath>

namespace rjo {

void MouseField::Reset(const Vec2& position) {
    state_ = {};
    state_.position = position;
    state_.previous = position;
    wake_.clear();
    initialized_ = true;
}

void MouseField::Update(float dt, const AppConfig& config, const RECT& overlayRect) {
    if (dt <= 0.0f) {
        return;
    }

    POINT pt{};
    GetCursorPos(&pt);

    // Convert absolute screen coords into overlay-local space.
    Vec2 absolute(static_cast<float>(pt.x), static_cast<float>(pt.y));
    Vec2 local(absolute.x - static_cast<float>(overlayRect.left),
               absolute.y - static_cast<float>(overlayRect.top));

    if (!initialized_) {
        Reset(local);
        return;
    }

    state_.previous = state_.position;
    state_.position = local;

    const Vec2 delta = state_.position - state_.previous;
    const float dist = delta.Length();
    Vec2 instantVelocity = delta * (1.0f / dt);

    // Clamp insane teleport spikes (monitor jumps / capture glitches).
    const float maxSpeed = 4000.0f;
    if (instantVelocity.Length() > maxSpeed) {
        instantVelocity = instantVelocity.Normalized() * maxSpeed;
    }

    state_.acceleration = (instantVelocity - state_.velocity) * (1.0f / dt);
    state_.velocity = instantVelocity;
    state_.smoothedVelocity = {
        ExpDecay(state_.smoothedVelocity.x, state_.velocity.x, config.cursorForceSmoothing, dt),
        ExpDecay(state_.smoothedVelocity.y, state_.velocity.y, config.cursorForceSmoothing, dt)
    };

    state_.movedThisFrame = dist > 0.5f;
    if (state_.movedThisFrame) {
        state_.idleTime = 0.0f;
        const float speed = state_.smoothedVelocity.Length();
        const float targetActivity = Clamp(speed / 900.0f, 0.0f, 1.0f);
        state_.activity = ExpDecay(state_.activity, targetActivity, 10.0f, dt);

        wake_.push_front({state_.position, state_.smoothedVelocity, 0.0f});
        while (wake_.size() > 24) {
            wake_.pop_back();
        }
    } else {
        state_.idleTime += dt;
        state_.activity = ExpDecay(state_.activity, 0.0f, 3.5f, dt);
        state_.smoothedVelocity = {
            ExpDecay(state_.smoothedVelocity.x, 0.0f, 4.0f, dt),
            ExpDecay(state_.smoothedVelocity.y, 0.0f, 4.0f, dt)
        };
    }

    for (auto& sample : wake_) {
        sample.time += dt;
    }
    while (!wake_.empty() && wake_.back().time > 0.35f) {
        wake_.pop_back();
    }
}

Vec2 MouseField::SampleForce(const Vec2& leafPos, const AppConfig& config) const {
    Vec2 force{};

    const float radius = config.cursorRadius;
    const Vec2 toLeaf = leafPos - state_.position;
    const float dist = toLeaf.Length();
    if (dist < radius * 2.5f && dist > 0.001f) {
        const float t = Clamp(dist / radius, 0.0f, 1.0f);
        const float falloff = 1.0f - SmoothStep(0.0f, 1.0f, t);
        const Vec2 dir = toLeaf * (1.0f / dist);
        const Vec2 tangent(-dir.y, dir.x);

        const float speed = state_.smoothedVelocity.Length();
        const float gust = Clamp(speed / 700.0f, 0.0f, 2.5f) * config.cursorGustScale;
        const float reaction = config.leafReactionStrength;

        // 1) Directional push from cursor velocity.
        force += state_.smoothedVelocity.Normalized() * (config.cursorWake * 0.35f * gust * falloff * reaction);

        // 2) Radial repulsion near cursor.
        const float nearFalloff = 1.0f - SmoothStep(0.0f, 0.55f, t);
        force += dir * (config.cursorRepulsion * nearFalloff * reaction);

        // 3) Weak vortex slightly farther out.
        const float vortexMask = SmoothStep(0.15f, 0.55f, t) * (1.0f - SmoothStep(0.55f, 1.0f, t));
        force += tangent * (config.cursorVortex * vortexMask * (0.35f + gust) * reaction);

        // 4) Turbulence from acceleration.
        const float accelMag = Clamp(state_.acceleration.Length() / 4000.0f, 0.0f, 1.5f);
        force += Vec2(dir.y, -dir.x) * (accelMag * 140.0f * falloff * reaction);
    }

    // 5) Fading wake behind recent cursor path.
    for (const auto& sample : wake_) {
        const Vec2 d = leafPos - sample.position;
        const float dLen = d.Length();
        const float wakeRadius = radius * 0.85f;
        if (dLen >= wakeRadius || dLen < 0.001f) {
            continue;
        }
        const float age = Clamp(sample.time / 0.35f, 0.0f, 1.0f);
        const float fall = (1.0f - SmoothStep(0.0f, 1.0f, dLen / wakeRadius)) * (1.0f - age);
        const float speed = sample.velocity.Length();
        if (speed < 1.0f) {
            continue;
        }
        force += sample.velocity.Normalized() * (config.cursorWake * fall * config.leafReactionStrength * 0.45f);
    }

    // Soft global activity scaling so idle cursor barely affects leaves.
    force *= (0.25f + 0.75f * state_.activity);
    return force;
}

} // namespace rjo
