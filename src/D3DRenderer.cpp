#include "D3DRenderer.h"
#include "Utilities/Logger.h"

#include <d3dcompiler.h>
#include <algorithm>
#include <fstream>
#include <vector>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace rjo {

namespace {

std::wstring GetShaderPath(const wchar_t* fileName) {
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    std::wstring dir(modulePath);
    const size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        dir = dir.substr(0, pos);
    }
    return dir + L"\\shaders\\" + fileName;
}

bool ReadFileBytes(const std::wstring& path, std::vector<char>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    in.seekg(0, std::ios::end);
    const auto size = static_cast<size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    out.resize(size);
    if (size > 0) {
        in.read(out.data(), static_cast<std::streamsize>(size));
    }
    return true;
}

HRESULT CompileShaderFromFile(const std::wstring& path, const char* entry, const char* target,
                              ID3DBlob** blob) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3DCompileFromFile(
        path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry, target, flags, 0, blob, errors.GetAddressOf());
    if (FAILED(hr) && errors) {
        Logger::Instance().Error(static_cast<const char*>(errors->GetBufferPointer()));
    }
    return hr;
}

bool CreateInputLayoutFromBlob(ID3D11Device* device, const void* bytecode, size_t size,
                               ID3D11InputLayout** layout) {
    D3D11_INPUT_ELEMENT_DESC desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"INSTANCE_POS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE_SIZE", 0, DXGI_FORMAT_R32_FLOAT, 1, 8, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE_ROT", 0, DXGI_FORMAT_R32_FLOAT, 1, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE_GLYPH", 0, DXGI_FORMAT_R32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE_GLOW", 0, DXGI_FORMAT_R32_FLOAT, 1, 36, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    };
    return SUCCEEDED(device->CreateInputLayout(desc, ARRAYSIZE(desc), bytecode, size, layout));
}

} // namespace

bool D3DRenderer::Initialize(HWND hwnd, UINT width, UINT height) {
    hwnd_ = hwnd;
    width_ = (std::max)(1u, width);
    height_ = (std::max)(1u, height);

    if (!CreateDevice()) return false;
    if (!CreatePipeline()) return false;
    if (!CreateResources()) return false;
    if (!CreateOffscreenTargets()) return false;
    if (!atlas_.Initialize(device_.Get(), 32)) return false;

    Logger::Instance().Info("D3D11 renderer initialized (UpdateLayeredWindow present)");
    return true;
}

void D3DRenderer::Shutdown() {
    atlas_.Release();
    ReleaseOffscreenTargets();
    depthState_.Reset();
    rasterState_.Reset();
    blendState_.Reset();
    sampler_.Reset();
    frameCB_.Reset();
    instanceVB_.Reset();
    quadVB_.Reset();
    inputLayout_.Reset();
    ps_.Reset();
    vs_.Reset();
    context_.Reset();
    device_.Reset();
}

bool D3DRenderer::CreateDevice() {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL got{};

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
        device_.ReleaseAndGetAddressOf(), &got, context_.ReleaseAndGetAddressOf());

#ifdef _DEBUG
    if (FAILED(hr)) {
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                               levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                               device_.ReleaseAndGetAddressOf(), &got,
                               context_.ReleaseAndGetAddressOf());
    }
#endif

    if (FAILED(hr)) {
        Logger::Instance().Error("D3D11CreateDevice failed");
        return false;
    }
    Logger::Instance().Info("Direct3D device created");
    return true;
}

bool D3DRenderer::CompileOrLoadShaders() {
    std::vector<char> vsBytes, psBytes;
    const std::wstring vsCso = GetShaderPath(L"AsciiVertexShader.cso");
    const std::wstring psCso = GetShaderPath(L"AsciiPixelShader.cso");

    if (ReadFileBytes(vsCso, vsBytes) && ReadFileBytes(psCso, psBytes)) {
        if (FAILED(device_->CreateVertexShader(vsBytes.data(), vsBytes.size(), nullptr, vs_.ReleaseAndGetAddressOf()))) {
            return false;
        }
        if (FAILED(device_->CreatePixelShader(psBytes.data(), psBytes.size(), nullptr, ps_.ReleaseAndGetAddressOf()))) {
            return false;
        }
        return CreateInputLayoutFromBlob(device_.Get(), vsBytes.data(), vsBytes.size(),
                                         inputLayout_.ReleaseAndGetAddressOf());
    }

    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;
    std::wstring vsPath = GetShaderPath(L"AsciiVertexShader.hlsl");
    std::wstring psPath = GetShaderPath(L"AsciiPixelShader.hlsl");
    if (GetFileAttributesW(vsPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        vsPath = L"shaders\\AsciiVertexShader.hlsl";
        psPath = L"shaders\\AsciiPixelShader.hlsl";
    }

    HRESULT hr = CompileShaderFromFile(vsPath, "main", "vs_5_0", vsBlob.GetAddressOf());
    if (FAILED(hr)) {
        hr = CompileShaderFromFile(L"..\\shaders\\AsciiVertexShader.hlsl", "main", "vs_5_0", vsBlob.GetAddressOf());
        if (FAILED(hr)) {
            Logger::Instance().Error("Vertex shader compilation failed");
            return false;
        }
        if (FAILED(CompileShaderFromFile(L"..\\shaders\\AsciiPixelShader.hlsl", "main", "ps_5_0", psBlob.GetAddressOf()))) {
            return false;
        }
    } else if (FAILED(CompileShaderFromFile(psPath, "main", "ps_5_0", psBlob.GetAddressOf()))) {
        Logger::Instance().Error("Pixel shader compilation failed");
        return false;
    }

    if (FAILED(device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vs_.ReleaseAndGetAddressOf()))) {
        return false;
    }
    if (FAILED(device_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, ps_.ReleaseAndGetAddressOf()))) {
        return false;
    }
    return CreateInputLayoutFromBlob(device_.Get(), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                     inputLayout_.ReleaseAndGetAddressOf());
}

bool D3DRenderer::CreatePipeline() {
    if (!CompileOrLoadShaders()) {
        return false;
    }

    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device_->CreateBlendState(&blend, blendState_.ReleaseAndGetAddressOf()))) {
        return false;
    }

    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = D3D11_FILL_SOLID;
    rs.CullMode = D3D11_CULL_NONE;
    rs.DepthClipEnable = TRUE;
    if (FAILED(device_->CreateRasterizerState(&rs, rasterState_.ReleaseAndGetAddressOf()))) {
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = FALSE;
    ds.StencilEnable = FALSE;
    if (FAILED(device_->CreateDepthStencilState(&ds, depthState_.ReleaseAndGetAddressOf()))) {
        return false;
    }

    D3D11_SAMPLER_DESC samp{};
    samp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.MaxLOD = D3D11_FLOAT32_MAX;
    return SUCCEEDED(device_->CreateSamplerState(&samp, sampler_.ReleaseAndGetAddressOf()));
}

bool D3DRenderer::CreateResources() {
    const float quad[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    D3D11_BUFFER_DESC vb{};
    vb.ByteWidth = sizeof(quad);
    vb.Usage = D3D11_USAGE_IMMUTABLE;
    vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA init{quad, 0, 0};
    if (FAILED(device_->CreateBuffer(&vb, &init, quadVB_.ReleaseAndGetAddressOf()))) {
        return false;
    }

    D3D11_BUFFER_DESC cb{};
    cb.ByteWidth = sizeof(FrameConstants);
    cb.Usage = D3D11_USAGE_DYNAMIC;
    cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device_->CreateBuffer(&cb, nullptr, frameCB_.ReleaseAndGetAddressOf()))) {
        return false;
    }

    instanceCapacity_ = 4096;
    D3D11_BUFFER_DESC ib{};
    ib.ByteWidth = instanceCapacity_ * sizeof(LeafInstanceGpu);
    ib.Usage = D3D11_USAGE_DYNAMIC;
    ib.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    ib.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(device_->CreateBuffer(&ib, nullptr, instanceVB_.ReleaseAndGetAddressOf()));
}

void D3DRenderer::ReleaseOffscreenTargets() {
    if (context_) {
        context_->OMSetRenderTargets(0, nullptr, nullptr);
    }
    rtv_.Reset();
    renderTex_.Reset();
    stagingTex_.Reset();

    if (dibDc_) {
        if (oldDib_) {
            SelectObject(dibDc_, oldDib_);
            oldDib_ = nullptr;
        }
        DeleteDC(dibDc_);
        dibDc_ = nullptr;
    }
    if (dib_) {
        DeleteObject(dib_);
        dib_ = nullptr;
        dibBits_ = nullptr;
    }
}

bool D3DRenderer::CreateOffscreenTargets() {
    ReleaseOffscreenTargets();

    D3D11_TEXTURE2D_DESC td{};
    td.Width = width_;
    td.Height = height_;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;

    if (FAILED(device_->CreateTexture2D(&td, nullptr, renderTex_.ReleaseAndGetAddressOf()))) {
        Logger::Instance().Error("Create render texture failed");
        return false;
    }
    if (FAILED(device_->CreateRenderTargetView(renderTex_.Get(), nullptr, rtv_.ReleaseAndGetAddressOf()))) {
        return false;
    }

    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(device_->CreateTexture2D(&td, nullptr, stagingTex_.ReleaseAndGetAddressOf()))) {
        Logger::Instance().Error("Create staging texture failed");
        return false;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(width_);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(height_); // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    dibDc_ = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    if (!dibDc_) {
        return false;
    }

    dib_ = CreateDIBSection(dibDc_, &bmi, DIB_RGB_COLORS, &dibBits_, nullptr, 0);
    if (!dib_ || !dibBits_) {
        Logger::Instance().Error("CreateDIBSection failed");
        return false;
    }
    oldDib_ = SelectObject(dibDc_, dib_);
    return true;
}

bool D3DRenderer::Resize(UINT width, UINT height) {
    width = (std::max)(1u, width);
    height = (std::max)(1u, height);
    if (width == width_ && height == height_) {
        return true;
    }
    width_ = width;
    height_ = height;
    return CreateOffscreenTargets();
}

bool D3DRenderer::HandleDeviceLost() {
    Logger::Instance().Warning("Device lost — recreating D3D resources");
    const UINT w = width_;
    const UINT h = height_;
    const HWND hwnd = hwnd_;
    Shutdown();
    return Initialize(hwnd, w, h);
}

bool D3DRenderer::UploadInstances(const void* data, UINT byteSize, UINT count) {
    if (count == 0) {
        return true;
    }
    if (count > instanceCapacity_) {
        instanceCapacity_ = count + 1024;
        D3D11_BUFFER_DESC ib{};
        ib.ByteWidth = instanceCapacity_ * sizeof(LeafInstanceGpu);
        ib.Usage = D3D11_USAGE_DYNAMIC;
        ib.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        ib.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device_->CreateBuffer(&ib, nullptr, instanceVB_.ReleaseAndGetAddressOf()))) {
            return false;
        }
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(instanceVB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        return false;
    }
    std::memcpy(mapped.pData, data, byteSize);
    context_->Unmap(instanceVB_.Get(), 0);
    return true;
}

bool D3DRenderer::PresentLayered() {
    if (!hwnd_ || !dibBits_ || !dibDc_ || !stagingTex_) {
        return false;
    }

    context_->CopyResource(stagingTex_.Get(), renderTex_.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(stagingTex_.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        return false;
    }

    const UINT dstPitch = width_ * 4;
    auto* dst = static_cast<uint8_t*>(dibBits_);
    const auto* src = static_cast<const uint8_t*>(mapped.pData);
    for (UINT y = 0; y < height_; ++y) {
        std::memcpy(dst + y * dstPitch, src + y * mapped.RowPitch, dstPitch);
    }
    context_->Unmap(stagingTex_.Get(), 0);

    RECT wnd{};
    GetWindowRect(hwnd_, &wnd);
    POINT ptDst{wnd.left, wnd.top};
    POINT ptSrc{0, 0};
    SIZE size{static_cast<LONG>(width_), static_cast<LONG>(height_)};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    // Per-pixel alpha layered update. Combined with WS_EX_TRANSPARENT,
    // mouse/hover always reaches windows underneath.
    if (!UpdateLayeredWindow(hwnd_, nullptr, &ptDst, &size, dibDc_, &ptSrc, 0, &blend, ULW_ALPHA)) {
        return false;
    }
    return true;
}

bool D3DRenderer::Render(const SceneRenderer& scene,
                         const std::vector<LeafInstanceGpu>& leaves,
                         const AppConfig& config,
                         float timeSeconds) {
    if (!rtv_) {
        return false;
    }

    const float clear[4] = {0, 0, 0, 0};
    context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(rtv_.Get(), clear);

    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(width_);
    vp.Height = static_cast<float>(height_);
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

    context_->IASetInputLayout(inputLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context_->VSSetShader(vs_.Get(), nullptr, 0);
    context_->PSSetShader(ps_.Get(), nullptr, 0);
    context_->OMSetBlendState(blendState_.Get(), nullptr, 0xFFFFFFFF);
    context_->OMSetDepthStencilState(depthState_.Get(), 0);
    context_->RSSetState(rasterState_.Get());

    ID3D11ShaderResourceView* srv = atlas_.Srv();
    context_->PSSetShaderResources(0, 1, &srv);
    context_->PSSetSamplers(0, 1, sampler_.GetAddressOf());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(context_->Map(frameCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        auto* cb = static_cast<FrameConstants*>(mapped.pData);
        cb->screenSize[0] = static_cast<float>(width_);
        cb->screenSize[1] = static_cast<float>(height_);
        cb->atlasSize[0] = static_cast<float>(atlas_.AtlasWidth());
        cb->atlasSize[1] = static_cast<float>(atlas_.AtlasHeight());
        cb->time = timeSeconds;
        cb->crtStrength = config.crtStrength;
        cb->scanlineStrength = config.scanlineStrength;
        cb->ditherStrength = config.ditherStrength;
        cb->grainStrength = config.grainStrength;
        cb->glowStrength = config.glowStrength;
        cb->glyphPixelSize = static_cast<float>(atlas_.GlyphPixelSize());
        cb->pad0 = 0.0f;
        context_->Unmap(frameCB_.Get(), 0);
    }
    context_->VSSetConstantBuffers(0, 1, frameCB_.GetAddressOf());
    context_->PSSetConstantBuffers(0, 1, frameCB_.GetAddressOf());

    UINT strides[2] = {sizeof(float) * 2, sizeof(LeafInstanceGpu)};
    UINT offsets[2] = {0, 0};
    ID3D11Buffer* buffers[2] = {quadVB_.Get(), instanceVB_.Get()};

    auto drawInstances = [&](const void* data, size_t count) {
        if (count == 0) return;
        UploadInstances(data, static_cast<UINT>(count * sizeof(LeafInstanceGpu)), static_cast<UINT>(count));
        context_->IASetVertexBuffers(0, 2, buffers, strides, offsets);
        context_->DrawInstanced(4, static_cast<UINT>(count), 0, 0);
    };

    const auto& sceneInst = scene.Instances();
    if (!sceneInst.empty()) {
        drawInstances(sceneInst.data(), sceneInst.size());
    }

    if (!leaves.empty()) {
        std::vector<LeafInstanceGpu> remapped = leaves;
        const uint32_t base = atlas_.LeafGlyphBase();
        for (auto& inst : remapped) {
            inst.glyphIndex = static_cast<float>(base + static_cast<uint32_t>(inst.glyphIndex) % atlas_.LeafGlyphCount());
        }
        drawInstances(remapped.data(), remapped.size());
    }

    // Flush GPU before CPU readback.
    context_->Flush();
    if (!PresentLayered()) {
        Logger::Instance().Warning("UpdateLayeredWindow failed");
        return false;
    }
    return true;
}

} // namespace rjo
