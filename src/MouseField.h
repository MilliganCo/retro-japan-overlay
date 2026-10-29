#pragma once

#include "Utilities/Math.h"
#include "Configuration.h"

#include <Windows.h>
#include <deque>

namespace rjo {

struct MouseSample {
    Vec2 position{};
    Vec2 velocity{};
    float time = 0.0f;
};

struct MouseState {
    Vec2 position{};          // screen / virtual-desktop pixels
    Vec2 previous{};
    Vec2 velocity{};
    Vec2 smoothedVelocity{};
    Vec2 acceleration{};
    float idleTime = 0.0f;
    float activity = 0.0f;    // 0..1 smoothed movement intensity
    bool movedThisFrame = false;
};

class MouseField {
public:
    void Reset(const Vec2& position);
    void Update(float dt, const AppConfig& config, const RECT& overlayRect);

    const MouseState& State() const { return state_; }

    // Force applied to a leaf at world/overlay-local position.
    Vec2 SampleForce(const Vec2& leafPos, const AppConfig& config) const;

private:
    MouseState state_{};
    std::deque<MouseSample> wake_;
    bool initialized_ = false;
};

} // namespace rjo
