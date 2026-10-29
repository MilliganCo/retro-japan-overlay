#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace rjo {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

    float LengthSq() const { return x * x + y * y; }
    float Length() const { return std::sqrt(LengthSq()); }

    Vec2 Normalized() const {
        const float len = Length();
        if (len < 1e-6f) {
            return {0.0f, 0.0f};
        }
        return {x / len, y / len};
    }
};

inline float Dot(const Vec2& a, const Vec2& b) {
    return a.x * b.x + a.y * b.y;
}

inline float Clamp(float v, float lo, float hi) {
    return (std::max)(lo, (std::min)(hi, v));
}

inline float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float SmoothStep(float edge0, float edge1, float x) {
    const float t = Clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float ExpDecay(float current, float target, float rate, float dt) {
    return Lerp(current, target, 1.0f - std::exp(-rate * dt));
}

inline float WrapAngle(float a) {
    constexpr float kTwoPi = 6.28318530718f;
    while (a > kTwoPi) a -= kTwoPi;
    while (a < 0.0f) a += kTwoPi;
    return a;
}

struct ColorRgba {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

inline uint32_t PackPremultipliedBgra8(const ColorRgba& c) {
    const float a = Clamp(c.a, 0.0f, 1.0f);
    const float r = Clamp(c.r * a, 0.0f, 1.0f);
    const float g = Clamp(c.g * a, 0.0f, 1.0f);
    const float b = Clamp(c.b * a, 0.0f, 1.0f);
    const uint8_t rb = static_cast<uint8_t>(r * 255.0f + 0.5f);
    const uint8_t gb = static_cast<uint8_t>(g * 255.0f + 0.5f);
    const uint8_t bb = static_cast<uint8_t>(b * 255.0f + 0.5f);
    const uint8_t ab = static_cast<uint8_t>(a * 255.0f + 0.5f);
    return (static_cast<uint32_t>(ab) << 24) |
           (static_cast<uint32_t>(rb) << 16) |
           (static_cast<uint32_t>(gb) << 8)  |
           (static_cast<uint32_t>(bb));
}

class XorShift32 {
public:
    explicit XorShift32(uint32_t seed = 0xA341316Cu) : state_(seed ? seed : 0xA341316Cu) {}

    uint32_t NextU32() {
        uint32_t x = state_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state_ = x;
        return x;
    }

    float NextFloat() {
        return (NextU32() & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
    }

    float Range(float lo, float hi) {
        return Lerp(lo, hi, NextFloat());
    }

private:
    uint32_t state_;
};

} // namespace rjo
