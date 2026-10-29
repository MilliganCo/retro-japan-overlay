#include "Utilities/ComHelpers.h"
#include "Utilities/Logger.h"

namespace rjo {

bool SucceededOrLog(HRESULT hr, const char* what) {
    if (SUCCEEDED(hr)) {
        return true;
    }
    char buf[256]{};
    sprintf_s(buf, "%s failed (HRESULT=0x%08X)", what, static_cast<unsigned>(hr));
    Logger::Instance().Error(buf);
    return false;
}

} // namespace rjo
