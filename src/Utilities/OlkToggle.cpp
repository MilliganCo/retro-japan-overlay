#include "Utilities/OlkToggle.h"

namespace rjo {

bool OlkToggle::Update(float dt) {
    idle_ += dt;
    if (idle_ > 1.6f) {
        stage_ = 0;
        idle_ = 0.0f;
    }

    const bool o = (GetAsyncKeyState('O') & 0x8000) != 0;
    const bool l = (GetAsyncKeyState('L') & 0x8000) != 0;
    const bool k = (GetAsyncKeyState('K') & 0x8000) != 0;

    const bool oPress = o && !oDown_;
    const bool lPress = l && !lDown_;
    const bool kPress = k && !kDown_;
    oDown_ = o;
    lDown_ = l;
    kDown_ = k;

    if (stage_ == 0) {
        if (oPress) {
            stage_ = 1;
            idle_ = 0.0f;
        }
        return false;
    }
    if (stage_ == 1) {
        if (lPress) {
            stage_ = 2;
            idle_ = 0.0f;
        } else if (oPress || kPress) {
            stage_ = oPress ? 1 : 0;
            idle_ = 0.0f;
        }
        return false;
    }
    // stage_ == 2
    if (kPress) {
        stage_ = 0;
        idle_ = 0.0f;
        return true;
    }
    if (oPress) {
        stage_ = 1;
        idle_ = 0.0f;
    } else if (lPress) {
        // stay waiting for K, reset idle
        idle_ = 0.0f;
    }
    return false;
}

} // namespace rjo
