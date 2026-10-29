#pragma once

#include <Windows.h>
#include <wrl/client.h>
#include <stdexcept>
#include <string>

namespace rjo {

using Microsoft::WRL::ComPtr;

inline void ThrowIfFailed(HRESULT hr, const char* what) {
    if (FAILED(hr)) {
        char buf[256]{};
        sprintf_s(buf, "%s (HRESULT=0x%08X)", what, static_cast<unsigned>(hr));
        throw std::runtime_error(buf);
    }
}

inline bool SucceededOrLog(HRESULT hr, const char* what);

} // namespace rjo
