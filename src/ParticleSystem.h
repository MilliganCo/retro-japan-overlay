#pragma once

#include "Configuration.h"
#include "MouseField.h"
#include "Utilities/Math.h"

#include <vector>
#include <cstdint>

namespace rjo {

struct LeafParticle {
    Vec2 position{};
    Vec2 velocity{};
    Vec2 acceleration{};
    float rotation = 0.0f;
    float angularVelocity = 0.0f;
    float scale = 1.0f;
    float opacity = 1.0f;
    float lifetime = 0.0f;
    float maxLifetime = 12.0f;
    float turbulencePhase = 0.0f;
    float dragCoeff = 1.0f;
    float massResponse = 1.0f;
    uint32_t glyphIndex = 0;
    ColorRgba color{};
    float glow = 0.0f;
    bool alive = false;
};

struct alignas(16) LeafInstanceGpu {
    float posX, posY;
    float size;
    float rotation;
    float colorR, colorG, colorB, colorA;
    float glyphIndex;
    float glow;
    float pad0, pad1;
};

class ParticleSystem {
public:
    void Initialize(int capacity);
    void Resize(int count);
    void ResetAll(float width, float height, const AppConfig& config);
    void RecolorAll(const AppConfig& config);
    void Update(float dt, float width, float height, const AppConfig& config, const MouseField& mouse);

    const std::vector<LeafParticle>& Particles() const { return particles_; }
    void BuildInstanceBuffer(std::vector<LeafInstanceGpu>& out, float leafOpacity) const;
    int AliveCount() const;

private:
    void Spawn(LeafParticle& p, float width, float height, bool fromTop, const AppConfig& config);
    ColorRgba PickLeafColor(XorShift32& rng, int palette) const;

    std::vector<LeafParticle> particles_;
    XorShift32 rng_{0xC0FFEEu};
    float time_ = 0.0f;
};

} // namespace rjo
