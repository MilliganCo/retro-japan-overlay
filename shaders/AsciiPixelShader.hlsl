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

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
    float Glow : TEXCOORD1;
};

Texture2D GlyphAtlas : register(t0);
SamplerState GlyphSampler : register(s0);

float Hash21(float2 p)
{
    float h = dot(p, float2(127.1, 311.7));
    return frac(sin(h) * 43758.5453);
}

float4 main(PSInput input) : SV_TARGET
{
    float4 glyph = GlyphAtlas.Sample(GlyphSampler, input.UV);
    float alpha = glyph.a;

    // Premultiplied output color.
    float3 rgb = input.Color.rgb * alpha;
    float a = input.Color.a * alpha;

    // Soft glow boost (still premultiplied).
    float glow = input.Glow * GlowStrength;
    rgb += rgb * glow * 0.65;
    a = saturate(a + glow * 0.08 * alpha);

    // Subtle scanlines.
    float scan = 1.0 - ScanlineStrength * 0.35 * (0.5 + 0.5 * sin(input.Position.y * 3.14159));
    rgb *= scan;

    // Mild CRT vignette-ish intensity (no opaque fill).
    float2 uvN = input.Position.xy / ScreenSize;
    float2 d = uvN - 0.5;
    float vignette = 1.0 - CrtStrength * 0.25 * dot(d, d) * 2.0;
    rgb *= vignette;

    // Ordered-ish dither / grain (very subtle).
    float noise = Hash21(input.Position.xy + Time);
    rgb += (noise - 0.5) * (DitherStrength * 0.04 + GrainStrength * 0.05);

    // Keep transparent where glyph is empty.
    rgb = max(rgb, 0.0);
    a = saturate(a);

    return float4(rgb, a);
}
