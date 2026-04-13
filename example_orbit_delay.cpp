#include <array>
#include <cmath>
#include <iostream>

#include "orbit_delay_core.h"

// Buffers externos estáticos (podem viver fora do objeto para integração embarcada/RT).
constexpr uint32_t MAX_DELAY_SAMPLES = 48000;
float delayBufferL[MAX_DELAY_SAMPLES];
float delayBufferR[MAX_DELAY_SAMPLES];

int main() {
    constexpr float sampleRate = 48000.0f;
    constexpr uint32_t numSamples = 512;

    orbit::dsp::OrbitDelayCore fx;
    if (!fx.attachBuffers(delayBufferL, MAX_DELAY_SAMPLES, delayBufferR, MAX_DELAY_SAMPLES)) {
        std::cerr << "Failed to attach delay buffers.\n";
        return 1;
    }

    // Default recomendado: float + interpolação linear (bom custo/qualidade e previsível em tempo real).
    // Trade-off opcional: ORBIT_DELAY_ENABLE_HERMITE melhora suavidade/sub-sample com custo extra de CPU.
    fx.reset(sampleRate);
    fx.setOrbit(0.73f);
    fx.setOffsetSamples(7200.0f);
    fx.setStereoSpread(32.0f);
    fx.setFeedback(0.55f);
    fx.setMix(0.4f);

    // Smear/diffuser: 0 estágio = bypass barato; mais estágios aumentam densidade e custo.
    fx.setDiffuserStages(3);
    fx.setDiffusion(0.3f);

    fx.setLowpassCutoffHz(5500.0f);
    fx.setDcBlockEnabled(true);

    std::array<float, numSamples> inL{};
    std::array<float, numSamples> inR{};
    std::array<float, numSamples> outL{};
    std::array<float, numSamples> outR{};

    for (uint32_t i = 0; i < numSamples; ++i) {
        const float phase = static_cast<float>(i) / sampleRate;
        const float signal = std::sin(2.0f * orbit::dsp::kPi * 440.0f * phase);
        inL[i] = signal;
        inR[i] = signal;
    }

    // processSampleStereo: menor latência de controle por amostra, porém com maior overhead de chamada.
    // processStereo (bloco): mesma DSP base com menor overhead total para buffers maiores.
    fx.processStereo(inL.data(), inR.data(), outL.data(), outR.data(), numSamples);

    // Pontos de extensão futura: cross-feedback L<->R, modulação de órbita/offset e seleção runtime de interpolação.
    std::cout << "Primeiras 8 amostras (L/R):\n";
    for (uint32_t i = 0; i < 8; ++i) {
        std::cout << i << ": " << outL[i] << " / " << outR[i] << '\n';
    }

    return 0;
}
