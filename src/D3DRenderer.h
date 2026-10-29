#pragma once

#include "AsciiAtlas.h"
#include "Configuration.h"
#include "ParticleSystem.h"
#include "SceneRenderer.h"
#include "Utilities/ComHelpers.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <vector>

namespace rjo {

struct FrameConstants {
    float screenSize[2];
    float atlasSize[2];
    float time;
    float crtStrength;
    float scanlineStrength;
    float ditherStrength;
    float grainStrength;
    float glowStrength;
    float glyphPixelSize;
    float pad0;
};

class D3DRenderer {
public:
    bool Initialize(HWND hwnd, UINT width, UINT height);
    void Shutdown();

    bool Resize(UINT width, UINT height);
    bool HandleDeviceLost();

    bool Render(const SceneRenderer& scene,
                const std::vector<LeafInstanceGpu>& leaves,
                const AppConfig& config,
                float timeSeconds);

    ID3D11Device* Device() const { return device_.Get(); }
    ID3D11DeviceContext* Context() const { return context_.Get(); }
    AsciiAtlas& Atlas() { return atlas_; }
    const AsciiAtlas& Atlas() const { return atlas_; }

    UINT Width() const { return width_; }
    UINT Height() const { return height_; }

private:
    bool CreateDevice();
    bool CreatePipeline();
    bool CreateResources();
    bool CreateOffscreenTargets();
    void ReleaseOffscreenTargets();
    bool UploadInstances(const void* data, UINT byteSize, UINT count);
    bool CompileOrLoadShaders();
    bool PresentLayered();

    HWND hwnd_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11Texture2D> renderTex_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    ComPtr<ID3D11Texture2D> stagingTex_;
    ComPtr<ID3D11VertexShader> vs_;
    ComPtr<ID3D11PixelShader> ps_;
    ComPtr<ID3D11InputLayout> inputLayout_;
    ComPtr<ID3D11Buffer> quadVB_;
    ComPtr<ID3D11Buffer> instanceVB_;
    ComPtr<ID3D11Buffer> frameCB_;
    ComPtr<ID3D11SamplerState> sampler_;
    ComPtr<ID3D11BlendState> blendState_;
    ComPtr<ID3D11RasterizerState> rasterState_;
    ComPtr<ID3D11DepthStencilState> depthState_;

    HBITMAP dib_ = nullptr;
    void* dibBits_ = nullptr;
    HDC dibDc_ = nullptr;
    HGDIOBJ oldDib_ = nullptr;

    AsciiAtlas atlas_;
    UINT instanceCapacity_ = 0;
};

} // namespace rjo
