#pragma once

#include "AsciiAtlas.h"
#include "Configuration.h"
#include "Utilities/Math.h"

#include <vector>
#include <cstdint>

namespace rjo {

struct alignas(16) SceneInstanceGpu {
    float posX, posY;
    float size;
    float rotation;
    float colorR, colorG, colorB, colorA;
    float glyphIndex;
    float glow;
    float pad0, pad1;
};

class SceneRenderer {
public:
    void Rebuild(float width, float height, int cellSize, const AsciiAtlas& atlas, const AppConfig& config);
    const std::vector<SceneInstanceGpu>& Instances() const { return instances_; }
    void Clear() { instances_.clear(); }

private:
    void Put(int gx, int gy, wchar_t ch, const ColorRgba& color, float glow, float opacity,
             int cols, int rows, float cell, const AsciiAtlas& atlas);
    ColorRgba SkyColor(float yNorm, float brightness) const;
    ColorRgba SunColor(float brightness) const;

    std::vector<SceneInstanceGpu> instances_;
};

} // namespace rjo
