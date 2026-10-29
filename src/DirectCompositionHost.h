#pragma once

#include "Utilities/ComHelpers.h"

#include <dcomp.h>
#include <dxgi1_2.h>
#include <d3d11.h>

namespace rjo {

class DirectCompositionHost {
public:
    bool Initialize(HWND hwnd, ID3D11Device* d3dDevice, IDXGISwapChain1* swapChain);
    void Shutdown();
    bool BindSwapChain(IDXGISwapChain1* swapChain);
    bool Commit();

private:
    HWND hwnd_ = nullptr;
    ComPtr<IDCompositionDevice> device_;
    ComPtr<IDCompositionTarget> target_;
    ComPtr<IDCompositionVisual> visual_;
};

} // namespace rjo
