#include "ParticleSystem.h"

#include <algorithm>
#include <cmath>

namespace rjo {

namespace {
constexpr char kLeafGlyphs[] = "*+x/\\<>~^.,'`%";
constexpr int kLeafGlyphCount = static_cast<int>(sizeof(kLeafGlyphs) - 1);
} // namespace

void ParticleSystem::Initialize(int capacity) {
    particles_.assign(static_cast<size_t>((std::max)(1, capacity)), {});
}

void ParticleSystem::Resize(int count) {
    count = (std::max)(1, count);
    if (static_cast<int>(particles_.size()) == count) {
        return;
    }
    particles_.resize(static_cast<size_t>(count));
}

void ParticleSystem::ResetAll(float width, float height, const AppConfig& config) {
    for (auto& p : particles_) {
        Spawn(p, width, height, false, config);
        p.lifetime = rng_.Range(0.0f, p.maxLifetime * 0.8f);
        p.opacity = Clamp(p.lifetime / 1.2f, 0.0f, 1.0f);
    }
}

void ParticleSystem::RecolorAll(const AppConfig& config) {
    for (auto& p : particles_) {
        if (p.alive) {
            p.color = PickLeafColor(rng_, config.leafPalette);
        }
    }
}

ColorRgba ParticleSystem::PickLeafColor(XorShift32& rng, int palette) const {
    const float pick = rng.NextFloat();
    switch (static_cast<LeafPalette>(palette)) {
    case LeafPalette::Sakura:
        if (pick < 0.35f) return {0.95f, 0.55f, 0.70f, 1.0f};
        if (pick < 0.70f) return {0.90f, 0.40f, 0.58f, 1.0f};
        return {0.98f, 0.75f, 0.82f, 1.0f};
    case LeafPalette::Amber:
        if (pick < 0.35f) return {0.95f, 0.72f, 0.25f, 1.0f};
        if (pick < 0.70f) return {0.90f, 0.55f, 0.15f, 1.0f};
        return {0.80f, 0.62f, 0.28f, 1.0f};
    case LeafPalette::Cyber:
        if (pick < 0.35f) return {0.20f, 0.90f, 0.85f, 1.0f};
        if (pick < 0.70f) return {0.35f, 0.75f, 1.00f, 1.0f};
        return {0.55f, 0.95f, 0.70f, 1.0f};
    case LeafPalette::Midnight:
        if (pick < 0.35f) return {0.35f, 0.45f, 0.85f, 1.0f};
        if (pick < 0.70f) return {0.45f, 0.35f, 0.75f, 1.0f};
        return {0.55f, 0.60f, 0.90f, 1.0f};
    case LeafPalette::Mono:
        if (pick < 0.5f) return {0.85f, 0.85f, 0.85f, 1.0f};
        return {0.65f, 0.65f, 0.65f, 1.0f};
    case LeafPalette::Crimson:
        if (pick < 0.35f) return {0.85f, 0.15f, 0.22f, 1.0f};
        if (pick < 0.70f) return {0.70f, 0.10f, 0.18f, 1.0f};
        return {0.95f, 0.30f, 0.28f, 1.0f};
    case LeafPalette::Autumn:
    default:
        if (pick < 0.28f) return {0.85f, 0.35f, 0.22f, 1.0f};
        if (pick < 0.55f) return {0.90f, 0.55f, 0.20f, 1.0f};
        if (pick < 0.78f) return {0.78f, 0.62f, 0.38f, 1.0f};
        return {0.55f, 0.22f, 0.18f, 1.0f};
    }
}

void ParticleSystem::Spawn(LeafParticle& p, float width, float height, bool fromTop, const AppConfig& config) {
    p.alive = true;
    p.position.x = rng_.Range(-40.0f, width + 40.0f);
    p.position.y = fromTop ? rng_.Range(-80.0f, -10.0f) : rng_.Range(-40.0f, height * 0.35f);
    p.velocity.x = rng_.Range(-25.0f, 25.0f) + config.baseWind * 0.15f;
    p.velocity.y = rng_.Range(10.0f, 40.0f);
    p.acceleration = {};
    p.rotation = rng_.Range(0.0f, 6.28318f);
    p.angularVelocity = rng_.Range(-1.8f, 1.8f);
    p.scale = rng_.Range(config.leafSizeMin, config.leafSizeMax);
    p.opacity = 0.0f;
    p.lifetime = 0.0f;
    p.maxLifetime = rng_.Range(8.0f, 18.0f);
    p.turbulencePhase = rng_.Range(0.0f, 6.28318f);
    p.dragCoeff = rng_.Range(0.75f, 1.35f);
    p.massResponse = rng_.Range(0.7f, 1.3f);
    p.glyphIndex = static_cast<uint32_t>(rng_.NextU32() % kLeafGlyphCount);
    p.color = PickLeafColor(rng_, config.leafPalette);
    p.glow = rng_.Range(0.0f, 0.35f);
}

void ParticleSystem::Update(float dt, float width, float height, const AppConfig& config, const MouseField& mouse) {
    dt = Clamp(dt, 0.0f, 0.05f) * config.animationSpeed;
    if (dt <= 0.0f) {
        return;
    }

    time_ += dt;
    const float wind = config.baseWind * config.windStrength;

    for (auto& p : particles_) {
        if (!p.alive) {
            Spawn(p, width, height, true, config);
        }

        p.lifetime += dt;

        const float fadeIn = Clamp(p.lifetime / 1.0f, 0.0f, 1.0f);
        const float fadeOut = Clamp((p.maxLifetime - p.lifetime) / 1.4f, 0.0f, 1.0f);
        p.opacity = fadeIn * fadeOut;

        Vec2 accel{};
        accel.y += config.gravity * p.massResponse;
        accel.x += wind;

        const float turb = config.turbulence;
        accel.x += std::sin(time_ * 1.7f + p.turbulencePhase) * turb * 0.35f;
        accel.y += std::cos(time_ * 1.1f + p.turbulencePhase * 0.7f) * turb * 0.12f;
        accel.x += std::sin(time_ * 3.3f + p.position.y * 0.01f + p.turbulencePhase) * turb * 0.2f;

        accel += mouse.SampleForce(p.position, config) * p.massResponse;

        p.acceleration = accel;
        p.velocity += accel * dt;
        const float drag = std::exp(-config.drag * p.dragCoeff * dt);
        p.velocity *= drag;

        const float speed = p.velocity.Length();
        if (speed > config.maxLeafSpeed) {
            p.velocity *= (config.maxLeafSpeed / speed);
        }

        p.position += p.velocity * dt;
        p.angularVelocity += std::sin(time_ + p.turbulencePhase) * 0.4f * dt;
        p.angularVelocity = Clamp(p.angularVelocity, -3.5f, 3.5f);
        p.rotation = WrapAngle(p.rotation + p.angularVelocity * dt);

        const bool offscreen =
            p.position.y > height + 60.0f ||
            p.position.x < -120.0f ||
            p.position.x > width + 120.0f ||
            p.lifetime > p.maxLifetime;

        if (offscreen) {
            Spawn(p, width, height, true, config);
        }
    }
}

void ParticleSystem::BuildInstanceBuffer(std::vector<LeafInstanceGpu>& out, float leafOpacity) const {
    out.clear();
    out.reserve(particles_.size());
    for (const auto& p : particles_) {
        if (!p.alive) {
            continue;
        }
        const float a = Clamp(p.opacity * leafOpacity, 0.0f, 1.0f);
        if (a < 0.01f) {
            continue;
        }
        LeafInstanceGpu inst{};
        inst.posX = p.position.x;
        inst.posY = p.position.y;
        inst.size = p.scale;
        inst.rotation = p.rotation;
        inst.colorR = p.color.r * a;
        inst.colorG = p.color.g * a;
        inst.colorB = p.color.b * a;
        inst.colorA = a;
        inst.glyphIndex = static_cast<float>(p.glyphIndex);
        inst.glow = p.glow;
        out.push_back(inst);
    }
}

int ParticleSystem::AliveCount() const {
    int n = 0;
    for (const auto& p : particles_) {
        if (p.alive && p.opacity > 0.01f) {
            ++n;
        }
    }
    return n;
}

} // namespace rjo
