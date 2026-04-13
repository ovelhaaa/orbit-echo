#include "core/include/orbit_delay_core.h"

#include <cstdint>

int main() {
    constexpr uint32_t kMaxDelaySamples = 48000;
    static float delayBuffer[kMaxDelaySamples] = {};

    orbit::dsp::OrbitDelayCore fx;
    fx.attachBuffers(delayBuffer, kMaxDelaySamples);
    fx.reset(48000.0f);
    fx.setFeedback(0.4f);

    volatile float out = fx.processSampleMono(0.0f);
    (void)out;
    return 0;
}
