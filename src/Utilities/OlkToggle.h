#pragma once

#include <Windows.h>

namespace rjo {

// Polls GetAsyncKeyState for the sequence O → L → K (no hooks, no focus steal).
class OlkToggle {
public:
    // Returns true once when the full sequence is completed.
    bool Update(float dt);

private:
    int stage_ = 0; // 0=O, 1=L, 2=K
    float idle_ = 0.0f;
    bool oDown_ = false;
    bool lDown_ = false;
    bool kDown_ = false;
};

} // namespace rjo
