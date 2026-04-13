#include <array>
#include <cmath>
#include <iostream>

#include "orbit_delay_core.h"

int main() {
    constexpr float sampleRate = 48000.0f;
    constexpr uint32_t delayBufferSize = 48000;
    constexpr uint32_t numSamples = 512;

    std::array<float, delayBufferSize> delayLeft{};
    std::array<float, delayBufferSize> delayRight{};

    orbit::dsp::OrbitDelayCore delay;
    delay.attachBuffers(delayLeft.data(), delayBufferSize, delayRight.data(), delayBufferSize);
    delay.reset(sampleRate);
    delay.setOrbit(0.73f);
    delay.setOffsetSamples(7200.0f);
    delay.setStereoSpread(32.0f);
    delay.setFeedback(0.55f);
    delay.setMix(0.4f);
    delay.setDiffuserStages(3);
    delay.setDiffusion(0.3f);
    delay.setLowpassCutoffHz(5500.0f);
    delay.setDcBlockEnabled(true);

    std::array<float, numSamples> inL{};
    std::array<float, numSamples> inR{};
    std::array<float, numSamples> outL{};
    std::array<float, numSamples> outR{};

    for (uint32_t i = 0; i < numSamples; ++i) {
        const float phase = static_cast<float>(i) / sampleRate;
        const float signal = std::sin(2.0f * 3.14159265358979323846f * 440.0f * phase);
        inL[i] = signal;
        inR[i] = signal;
    }

    delay.processStereo(inL.data(), inR.data(), outL.data(), outR.data(), numSamples);

    std::cout << "Primeiras 8 amostras (L/R):\n";
    for (uint32_t i = 0; i < 8; ++i) {
        std::cout << i << ": " << outL[i] << " / " << outR[i] << '\n';
    }

    return 0;
}
