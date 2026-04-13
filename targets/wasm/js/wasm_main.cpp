#include "core/include/orbit_delay_c_api.h"

#include <cstdint>

int main() {
    constexpr uint32_t kMaxDelaySamples = 48000;
    static float delayBufferL[kMaxDelaySamples] = {};
    static float delayBufferR[kMaxDelaySamples] = {};

    OrbitDelayHandle* fx = orbit_init(48000.0f, delayBufferL, kMaxDelaySamples, delayBufferR, kMaxDelaySamples);
    if (fx == nullptr) {
        return 1;
    }

    orbit_set_feedback(fx, 0.4f);
    orbit_set_mix(fx, 0.35f);
    orbit_set_tone_hz(fx, 6500.0f);
    orbit_set_smear_amount(fx, 0.25f);

    const float inL[1] = {0.0f};
    const float inR[1] = {0.0f};
    float outL[1] = {0.0f};
    float outR[1] = {0.0f};
    const bool ok = orbit_process_stereo(fx, inL, inR, outL, outR, 1);

    orbit_free(fx);
    return ok ? 0 : 2;
}
