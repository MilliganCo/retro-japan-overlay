#pragma once

#include <Windows.h>
#include <cstdint>

namespace rjo {

class HighResolutionTimer {
public:
    HighResolutionTimer() {
        QueryPerformanceFrequency(&frequency_);
        QueryPerformanceCounter(&last_);
    }

    void Reset() {
        QueryPerformanceCounter(&last_);
    }

    // Returns elapsed seconds since last Tick/Reset and advances the clock.
    double Tick() {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        const double dt = static_cast<double>(now.QuadPart - last_.QuadPart) /
                          static_cast<double>(frequency_.QuadPart);
        last_ = now;
        return dt;
    }

    double ElapsedSinceReset() const {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        return static_cast<double>(now.QuadPart - last_.QuadPart) /
               static_cast<double>(frequency_.QuadPart);
    }

    void WaitUntil(double targetSeconds) const {
        if (targetSeconds <= 0.0) {
            return;
        }
        for (;;) {
            const double elapsed = ElapsedSinceReset();
            if (elapsed >= targetSeconds) {
                break;
            }
            const double remaining = targetSeconds - elapsed;
            if (remaining > 0.002) {
                Sleep(1);
            } else {
                // Busy-wait the final sub-millisecond for accuracy without spinning hard.
                YieldProcessor();
            }
        }
    }

private:
    LARGE_INTEGER frequency_{};
    LARGE_INTEGER last_{};
};

} // namespace rjo
