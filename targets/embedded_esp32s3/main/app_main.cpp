#include "core/include/orbit_delay_core.h"

#include <cstdint>

extern "C" void app_main(void) {
    constexpr uint32_t kMaxDelaySamples = 48000;
    static float delayBufferL[kMaxDelaySamples] = {};
    static float delayBufferR[kMaxDelaySamples] = {};

    orbit::dsp::OrbitDelayCore fx;
    fx.attachBuffers(delayBufferL, kMaxDelaySamples, delayBufferR, kMaxDelaySamples);
    fx.reset(48000.0f);
    fx.setFeedback(0.45f);
    fx.setMix(0.35f);

    float outL = 0.0f;
    float outR = 0.0f;
    fx.processSampleStereo(0.0f, 0.0f, outL, outR);
}
