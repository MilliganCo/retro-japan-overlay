#include "AsciiAtlas.h"
#include "Utilities/Logger.h"

#include <algorithm>
#include <cstring>

namespace rjo {

namespace {
constexpr wchar_t kSceneCharset[] =
    L" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
    L"abcdefghijklmnopqrstuvwxyz{|}~"
    L".,'`%*+x/\\<>~^"
    L"█▓▒░╳╱╲│─┌┐└┘├┤┬┴┼▲▼◆○●◎";

constexpr wchar_t kLeafChars[] = L"*+x/\\<>~^.,'`%";
}

bool AsciiAtlas::Initialize(ID3D11Device* device, int glyphPixelSize) {
    glyphPixelSize_ = (std::max)(8, glyphPixelSize);
    charset_.assign(kSceneCharset, kSceneCharset + wcslen(kSceneCharset));

    // Ensure leaf glyphs are present and contiguous for particle indexing.
    leafGlyphBase_ = static_cast<uint32_t>(charset_.size());
    for (const wchar_t* p = kLeafChars; *p; ++p) {
        charset_.push_back(*p);
    }
    leafGlyphCount_ = static_cast<uint32_t>(wcslen(kLeafChars));

    charToIndex_.clear();
    for (uint32_t i = 0; i < charset_.size(); ++i) {
        if (charToIndex_.find(charset_[i]) == charToIndex_.end()) {
            charToIndex_[charset_[i]] = i;
        }
    }

    if (!Rasterize(device)) {
        Logger::Instance().Error("AsciiAtlas rasterization failed");
        return false;
    }

    Logger::Instance().Info("Ascii glyph atlas created");
    return true;
}

void AsciiAtlas::Release() {
    srv_.Reset();
    texture_.Reset();
    glyphs_.clear();
    charToIndex_.clear();
    charset_.clear();
}

bool AsciiAtlas::Find(wchar_t ch, GlyphInfo& out) const {
    const auto it = charToIndex_.find(ch);
    if (it == charToIndex_.end()) {
        return false;
    }
    out = glyphs_[it->second];
    return true;
}

uint32_t AsciiAtlas::IndexOf(wchar_t ch) const {
    const auto it = charToIndex_.find(ch);
    if (it == charToIndex_.end()) {
        return 0;
    }
    return it->second;
}

wchar_t AsciiAtlas::CharAt(uint32_t index) const {
    if (index >= charset_.size()) {
        return L' ';
    }
    return charset_[index];
}

bool AsciiAtlas::Rasterize(ID3D11Device* device) {
    const int count = static_cast<int>(charset_.size());
    const int cols = 32;
    const int rows = (count + cols - 1) / cols;
    const int cell = glyphPixelSize_;
    atlasWidth_ = cols * cell;
    atlasHeight_ = rows * cell;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = atlasWidth_;
    bmi.bmiHeader.biHeight = -atlasHeight_; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HDC hdc = CreateCompatibleDC(screen);
    HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
        if (hdc) DeleteDC(hdc);
        ReleaseDC(nullptr, screen);
        return false;
    }

    HGDIOBJ oldBmp = SelectObject(hdc, dib);
    RECT fill{0, 0, atlasWidth_, atlasHeight_};
    FillRect(hdc, &fill, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    // Prefer Cascadia Mono, fall back to Consolas / Courier New.
    HFONT font = CreateFontW(
        cell - 2, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Mono");
    if (!font) {
        font = CreateFontW(cell - 2, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    }
    if (!font) {
        font = CreateFontW(cell - 2, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier New");
    }

    HGDIOBJ oldFont = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    glyphs_.assign(static_cast<size_t>(count), {});
    for (int i = 0; i < count; ++i) {
        const int col = i % cols;
        const int row = i / cols;
        const int x = col * cell;
        const int y = row * cell;
        wchar_t ch[2] = {charset_[static_cast<size_t>(i)], 0};
        TextOutW(hdc, x + 1, y, ch, 1);

        GlyphInfo g;
        g.ch = charset_[static_cast<size_t>(i)];
        g.width = static_cast<float>(cell);
        g.height = static_cast<float>(cell);
        g.u0 = static_cast<float>(x) / static_cast<float>(atlasWidth_);
        g.v0 = static_cast<float>(y) / static_cast<float>(atlasHeight_);
        g.u1 = static_cast<float>(x + cell) / static_cast<float>(atlasWidth_);
        g.v1 = static_cast<float>(y + cell) / static_cast<float>(atlasHeight_);
        glyphs_[static_cast<size_t>(i)] = g;
    }

    // Convert RGB luminance into alpha-only style (store white RGB + alpha from luminance).
    auto* pixels = static_cast<uint32_t*>(bits);
    const int total = atlasWidth_ * atlasHeight_;
    for (int i = 0; i < total; ++i) {
        const uint32_t p = pixels[i];
        const uint8_t b = static_cast<uint8_t>(p & 0xFF);
        const uint8_t g = static_cast<uint8_t>((p >> 8) & 0xFF);
        const uint8_t r = static_cast<uint8_t>((p >> 16) & 0xFF);
        const uint8_t a = static_cast<uint8_t>((0.2126f * r + 0.7152f * g + 0.0722f * b) + 0.5f);
        // Premultiplied white glyph.
        pixels[i] = (static_cast<uint32_t>(a) << 24) |
                    (static_cast<uint32_t>(a) << 16) |
                    (static_cast<uint32_t>(a) << 8) |
                    (static_cast<uint32_t>(a));
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(atlasWidth_);
    desc.Height = static_cast<UINT>(atlasHeight_);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = bits;
    init.SysMemPitch = static_cast<UINT>(atlasWidth_ * 4);

    HRESULT hr = device->CreateTexture2D(&desc, &init, texture_.ReleaseAndGetAddressOf());
    if (SUCCEEDED(hr)) {
        hr = device->CreateShaderResourceView(texture_.Get(), nullptr, srv_.ReleaseAndGetAddressOf());
    }

    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldBmp);
    if (font) DeleteObject(font);
    DeleteObject(dib);
    DeleteDC(hdc);
    ReleaseDC(nullptr, screen);

    return SUCCEEDED(hr);
}

} // namespace rjo
