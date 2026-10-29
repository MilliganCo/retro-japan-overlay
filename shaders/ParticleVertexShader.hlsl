cbuffer FrameCB : register(b0)
{
    float2 ScreenSize;
    float2 AtlasSize;
    float Time;
    float CrtStrength;
    float ScanlineStrength;
    float DitherStrength;
    float GrainStrength;
    float GlowStrength;
    float GlyphPixelSize;
    float Pad0;
};

struct VSInput
{
    // Vertex quad (unit square corner)
    float2 Corner : POSITION;
    // Per-instance
    float2 Pos : INSTANCE_POS;
    float Size : INSTANCE_SIZE;
    float Rotation : INSTANCE_ROT;
    float4 Color : INSTANCE_COLOR;
    float GlyphIndex : INSTANCE_GLYPH;
    float Glow : INSTANCE_GLOW;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
    float Glow : TEXCOORD1;
};

Texture2D GlyphAtlas : register(t0);
SamplerState GlyphSampler : register(s0);

PSInput main(VSInput input)
{
    PSInput o;

    float c = cos(input.Rotation);
    float s = sin(input.Rotation);
    float2 local = (input.Corner - 0.5.xx) * input.Size;
    float2 rotated = float2(local.x * c - local.y * s, local.x * s + local.y * c);
    float2 pixel = input.Pos + rotated;

    float2 ndc;
    ndc.x = (pixel.x / ScreenSize.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (pixel.y / ScreenSize.y) * 2.0;
    o.Position = float4(ndc, 0.0, 1.0);

    // Atlas UV from glyph index (32 columns).
    uint glyph = (uint)input.GlyphIndex;
    uint col = glyph % 32;
    uint row = glyph / 32;
    float2 cellUV = float2((float)col, (float)row) * GlyphPixelSize / AtlasSize;
    float2 cellSizeUV = GlyphPixelSize / AtlasSize;
    o.UV = cellUV + input.Corner * cellSizeUV;

    o.Color = input.Color;
    o.Glow = input.Glow;
    return o;
}
