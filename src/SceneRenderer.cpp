#include "SceneRenderer.h"

#include <cmath>
#include <algorithm>

namespace rjo {

ColorRgba SceneRenderer::SkyColor(float yNorm, float brightness) const {
    // Top: dark navy transparent, bottom: muted red-orange near horizon.
    const ColorRgba top{0.08f, 0.10f, 0.18f, 0.22f};
    const ColorRgba mid{0.35f, 0.12f, 0.16f, 0.35f};
    const ColorRgba low{0.55f, 0.22f, 0.14f, 0.42f};

    ColorRgba c;
    if (yNorm < 0.55f) {
        const float t = yNorm / 0.55f;
        c.r = Lerp(top.r, mid.r, t);
        c.g = Lerp(top.g, mid.g, t);
        c.b = Lerp(top.b, mid.b, t);
        c.a = Lerp(top.a, mid.a, t);
    } else {
        const float t = (yNorm - 0.55f) / 0.45f;
        c.r = Lerp(mid.r, low.r, t);
        c.g = Lerp(mid.g, low.g, t);
        c.b = Lerp(mid.b, low.b, t);
        c.a = Lerp(mid.a, low.a, t);
    }
    c.r *= brightness;
    c.g *= brightness;
    c.b *= brightness;
    return c;
}

ColorRgba SceneRenderer::SunColor(float brightness) const {
    return {Clamp(0.95f * brightness, 0.0f, 1.0f),
            Clamp(0.42f * brightness, 0.0f, 1.0f),
            Clamp(0.18f * brightness, 0.0f, 1.0f),
            0.85f};
}

void SceneRenderer::Put(int gx, int gy, wchar_t ch, const ColorRgba& color, float glow, float opacity,
                        int cols, int rows, float cell, const AsciiAtlas& atlas) {
    if (gx < 0 || gy < 0 || gx >= cols || gy >= rows) {
        return;
    }
    GlyphInfo g{};
    if (!atlas.Find(ch, g)) {
        ch = L'#';
        if (!atlas.Find(ch, g)) {
            return;
        }
    }

    const float a = Clamp(color.a * opacity, 0.0f, 1.0f);
    if (a < 0.01f) {
        return;
    }

    SceneInstanceGpu inst{};
    inst.posX = (static_cast<float>(gx) + 0.5f) * cell;
    inst.posY = (static_cast<float>(gy) + 0.5f) * cell;
    inst.size = cell * 1.05f;
    inst.rotation = 0.0f;
    inst.colorR = color.r * a;
    inst.colorG = color.g * a;
    inst.colorB = color.b * a;
    inst.colorA = a;
    inst.glyphIndex = static_cast<float>(atlas.IndexOf(ch));
    inst.glow = glow;
    instances_.push_back(inst);
}

void SceneRenderer::Rebuild(float width, float height, int cellSize, const AsciiAtlas& atlas, const AppConfig& config) {
    instances_.clear();
    if (!config.showScene || config.sceneOpacity <= 0.01f) {
        return;
    }

    cellSize = (std::max)(8, cellSize);
    const float cell = static_cast<float>(cellSize);
    const int cols = (std::max)(16, static_cast<int>(width / cell));
    const int rows = (std::max)(10, static_cast<int>(height / cell));
    instances_.reserve(static_cast<size_t>(cols * rows / 3));

    const float brightness = config.sceneBrightness;
    const float opacity = config.sceneOpacity * config.overlayOpacity;

    // Sparse atmospheric sky field.
    for (int y = 0; y < rows; ++y) {
        const float yNorm = static_cast<float>(y) / static_cast<float>(rows - 1);
        for (int x = 0; x < cols; ++x) {
            // Sparse dithered atmosphere, denser near horizon.
            const int hash = (x * 73856093) ^ (y * 19349663);
            const float dens = 0.04f + yNorm * 0.12f;
            if (((hash >> 3) & 255) / 255.0f > dens) {
                continue;
            }
            wchar_t ch = L'.';
            const int pick = (hash >> 8) & 7;
            if (pick == 0) ch = L':';
            else if (pick == 1) ch = L'+';
            else if (pick == 2) ch = L'*';
            else if (pick == 3) ch = L'`';
            ColorRgba c = SkyColor(yNorm, brightness);
            c.a *= 0.55f;
            Put(x, y, ch, c, 0.0f, opacity, cols, rows, cell, atlas);
        }
    }

    // Large ASCII sun near horizon, slightly right of center.
    const int sunCx = static_cast<int>(cols * 0.62f);
    const int sunCy = static_cast<int>(rows * 0.58f);
    const float sunR = (std::min)(cols, rows) * 0.16f;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const float dx = (x - sunCx) / sunR;
            const float dy = (y - sunCy) / (sunR * 0.92f);
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d > 1.05f) {
                continue;
            }
            wchar_t ch = L'@';
            if (d > 0.92f) ch = L'O';
            else if (d > 0.75f) ch = L'0';
            else if (d > 0.45f) ch = L'%';
            else ch = L'#';

            ColorRgba c = SunColor(brightness);
            c.a *= (1.0f - SmoothStep(0.75f, 1.05f, d));
            const float glow = (1.0f - d) * 0.8f;
            Put(x, y, ch, c, glow, opacity, cols, rows, cell, atlas);
        }
    }

    // Sparse clouds.
    const int cloudRows[] = {
        static_cast<int>(rows * 0.18f),
        static_cast<int>(rows * 0.26f),
        static_cast<int>(rows * 0.33f)
    };
    for (int ci = 0; ci < 3; ++ci) {
        const int baseY = cloudRows[ci];
        const int baseX = static_cast<int>(cols * (0.15f + ci * 0.28f));
        const wchar_t cloudGlyphs[] = L".,-~*~-,.";
        for (int i = 0; cloudGlyphs[i]; ++i) {
            ColorRgba c{0.70f, 0.45f, 0.40f, 0.28f};
            c.r *= brightness; c.g *= brightness; c.b *= brightness;
            Put(baseX + i, baseY + ((i % 3) - 1), cloudGlyphs[i], c, 0.05f, opacity, cols, rows, cell, atlas);
        }
    }

    // Mount Fuji-inspired mountain silhouette.
    const int horizon = static_cast<int>(rows * 0.68f);
    const int peakX = static_cast<int>(cols * 0.38f);
    const int peakY = static_cast<int>(rows * 0.42f);
    for (int x = 0; x < cols; ++x) {
        float ridge = static_cast<float>(horizon);
        // Left slope
        if (x < peakX) {
            const float t = static_cast<float>(x) / static_cast<float>((std::max)(1, peakX));
            ridge = Lerp(static_cast<float>(horizon), static_cast<float>(peakY), std::pow(t, 1.15f));
            // Soft secondary hill.
            ridge -= std::sin(t * 3.14159f) * (rows * 0.03f);
        } else {
            const float t = static_cast<float>(x - peakX) / static_cast<float>((std::max)(1, cols - peakX));
            ridge = Lerp(static_cast<float>(peakY), static_cast<float>(horizon + rows * 0.02f), std::pow(t, 0.9f));
        }

        // Snow cap near peak.
        const bool nearPeak = std::abs(x - peakX) < cols * 0.05f;
        for (int y = static_cast<int>(ridge); y < rows; ++y) {
            wchar_t ch = L'#';
            ColorRgba c{0.05f, 0.06f, 0.10f, 0.72f};
            if (nearPeak && y < static_cast<int>(ridge) + 2) {
                ch = L'^';
                c = {0.75f, 0.70f, 0.62f, 0.55f};
            } else if ((x + y) % 5 == 0) {
                ch = L'%';
            } else if ((x * 3 + y) % 7 == 0) {
                ch = L'X';
            }
            c.r *= brightness * 0.85f;
            c.g *= brightness * 0.85f;
            c.b *= brightness * 0.85f;
            Put(x, y, ch, c, 0.0f, opacity, cols, rows, cell, atlas);
        }
    }

    // Torii gate silhouette (foreground-ish, left-center).
    const int toriiX = static_cast<int>(cols * 0.22f);
    const int toriiBase = static_cast<int>(rows * 0.78f);
    const int toriiH = static_cast<int>(rows * 0.20f);
    const int toriiW = static_cast<int>(cols * 0.12f);
    const ColorRgba toriiColor{0.03f, 0.03f, 0.05f, 0.80f};
    // Vertical pillars
    for (int y = toriiBase - toriiH; y <= toriiBase; ++y) {
        Put(toriiX, y, L'|', toriiColor, 0.0f, opacity, cols, rows, cell, atlas);
        Put(toriiX + toriiW, y, L'|', toriiColor, 0.0f, opacity, cols, rows, cell, atlas);
    }
    // Top lintels
    for (int x = toriiX - 1; x <= toriiX + toriiW + 1; ++x) {
        Put(x, toriiBase - toriiH, L'=', toriiColor, 0.0f, opacity, cols, rows, cell, atlas);
        Put(x, toriiBase - toriiH + 1, L'=', toriiColor, 0.0f, opacity, cols, rows, cell, atlas);
    }
    // Crossbar
    for (int x = toriiX; x <= toriiX + toriiW; ++x) {
        Put(x, toriiBase - toriiH + static_cast<int>(toriiH * 0.35f), L'-', toriiColor, 0.0f, opacity, cols, rows, cell, atlas);
    }

    // Water / reflection band under horizon.
    for (int y = horizon; y < rows; ++y) {
        const float yNorm = static_cast<float>(y - horizon) / static_cast<float>((std::max)(1, rows - horizon));
        for (int x = 0; x < cols; ++x) {
            const int hash = (x * 83492791) ^ (y * 2654435761u);
            if (((hash >> 5) & 255) / 255.0f > 0.22f + (1.0f - yNorm) * 0.2f) {
                continue;
            }
            wchar_t ch = L'~';
            if ((hash & 3) == 0) ch = L'-';
            else if ((hash & 3) == 1) ch = L'=';
            else if ((hash & 3) == 2) ch = L'.';

            // Reflect sun colors more strongly under the sun.
            const float sunDist = std::abs(x - sunCx) / static_cast<float>(cols);
            ColorRgba c{0.45f, 0.18f, 0.12f, 0.28f};
            if (sunDist < 0.18f) {
                c = {0.80f, 0.35f, 0.16f, 0.34f};
                ch = L'%';
            }
            c.a *= (1.0f - yNorm * 0.65f);
            c.r *= brightness; c.g *= brightness; c.b *= brightness;
            Put(x, y, ch, c, sunDist < 0.15f ? 0.25f : 0.0f, opacity * 0.9f, cols, rows, cell, atlas);
        }
    }
}

} // namespace rjo
