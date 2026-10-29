#include "DirectCompositionHost.h"
#include "Utilities/Logger.h"

#include <dxgi.h>

#pragma comment(lib, "dcomp.lib")

namespace rjo {

bool DirectCompositionHost::Initialize(HWND hwnd, ID3D11Device* d3dDevice, IDXGISwapChain1* swapChain) {
    hwnd_ = hwnd;
    if (!d3dDevice || !swapChain) {
        Logger::Instance().Error("DirectComposition Initialize: null device/swapChain");
        return false;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = d3dDevice->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()));
    if (FAILED(hr)) {
        Logger::Instance().Error("QueryInterface IDXGIDevice failed");
        return false;
    }

    hr = DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(device_.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        Logger::Instance().Error("DCompositionCreateDevice failed");
        return false;
    }

    hr = device_->CreateTargetForHwnd(hwnd, TRUE, target_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        Logger::Instance().Error("CreateTargetForHwnd failed");
        return false;
    }

    hr = device_->CreateVisual(visual_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        Logger::Instance().Error("CreateVisual failed");
        return false;
    }

    if (!BindSwapChain(swapChain)) {
        return false;
    }

    hr = target_->SetRoot(visual_.Get());
    if (FAILED(hr)) {
        return false;
    }

    if (!Commit()) {
        return false;
    }

    Logger::Instance().Info("DirectComposition initialized");
    return true;
}

void DirectCompositionHost::Shutdown() {
    if (target_) {
        target_->SetRoot(nullptr);
    }
    if (device_) {
        device_->Commit();
    }
    visual_.Reset();
    target_.Reset();
    device_.Reset();
    hwnd_ = nullptr;
}

bool DirectCompositionHost::BindSwapChain(IDXGISwapChain1* swapChain) {
    if (!visual_ || !swapChain) {
        return false;
    }
    const HRESULT hr = visual_->SetContent(swapChain);
    if (FAILED(hr)) {
        Logger::Instance().Error("DComp Visual SetContent failed");
        return false;
    }
    return true;
}

bool DirectCompositionHost::Commit() {
    if (!device_) {
        return true;
    }
    const HRESULT hr = device_->Commit();
    if (FAILED(hr)) {
        Logger::Instance().Error("DComp Commit failed");
        return false;
    }
    return true;
}

} // namespace rjo
