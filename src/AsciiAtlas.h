#pragma once

#include "Utilities/ComHelpers.h"
#include "Utilities/Math.h"

#include <d3d11.h>
#include <vector>
#include <string>
#include <unordered_map>

namespace rjo {

struct GlyphInfo {
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    float width = 0;
    float height = 0;
    wchar_t ch = 0;
};

class AsciiAtlas {
public:
    bool Initialize(ID3D11Device* device, int glyphPixelSize);
    void Release();

    ID3D11ShaderResourceView* Srv() const { return srv_.Get(); }
    ID3D11Texture2D* Texture() const { return texture_.Get(); }
    int GlyphPixelSize() const { return glyphPixelSize_; }
    int AtlasWidth() const { return atlasWidth_; }
    int AtlasHeight() const { return atlasHeight_; }

    bool Find(wchar_t ch, GlyphInfo& out) const;
    uint32_t IndexOf(wchar_t ch) const;
    wchar_t CharAt(uint32_t index) const;
    uint32_t GlyphCount() const { return static_cast<uint32_t>(charset_.size()); }

    // Leaf glyph subset starts at this index in the atlas charset.
    uint32_t LeafGlyphBase() const { return leafGlyphBase_; }
    uint32_t LeafGlyphCount() const { return leafGlyphCount_; }

private:
    bool Rasterize(ID3D11Device* device);

    ComPtr<ID3D11Texture2D> texture_;
    ComPtr<ID3D11ShaderResourceView> srv_;
    std::vector<wchar_t> charset_;
    std::unordered_map<wchar_t, uint32_t> charToIndex_;
    std::vector<GlyphInfo> glyphs_;
    int glyphPixelSize_ = 16;
    int atlasWidth_ = 0;
    int atlasHeight_ = 0;
    uint32_t leafGlyphBase_ = 0;
    uint32_t leafGlyphCount_ = 0;
};

} // namespace rjo
