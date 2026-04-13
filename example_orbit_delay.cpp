#include <array>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

#include "core/include/orbit_delay_core.h"

// Buffers externos estáticos (podem viver fora do objeto para integração embarcada/RT).
constexpr uint32_t MAX_DELAY_SAMPLES = 48000;
float delayBufferL[MAX_DELAY_SAMPLES];
float delayBufferR[MAX_DELAY_SAMPLES];

int main() {
    constexpr float sampleRate = 48000.0f;
    constexpr uint32_t numSamples = 512;

    orbit::dsp::OrbitDelayCore fx;
    if (!fx.attachBuffers(delayBufferL, delayBufferR, MAX_DELAY_SAMPLES)) {
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
    fx.setSmearAmount(0.3f);

    fx.setToneHz(5500.0f);
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

    auto runBlockBenchmark = [&](uint32_t blockSize, uint32_t iterations) {
        std::vector<float> benchInL(blockSize);
        std::vector<float> benchInR(blockSize);
        std::vector<float> benchOutL(blockSize);
        std::vector<float> benchOutR(blockSize);

        for (uint32_t i = 0; i < blockSize; ++i) {
            const float phase = static_cast<float>(i) / sampleRate;
            const float signal = std::sin(2.0f * orbit::dsp::kPi * 220.0f * phase);
            benchInL[i] = signal;
            benchInR[i] = signal;
        }

        const auto t0 = std::chrono::high_resolution_clock::now();
        for (uint32_t it = 0; it < iterations; ++it) {
            fx.processStereo(benchInL.data(), benchInR.data(), benchOutL.data(), benchOutR.data(), blockSize);
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        const auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        const double nsPerSample = static_cast<double>(elapsedNs) / static_cast<double>(iterations * blockSize * 2u);
        std::cout << "Benchmark bloco " << std::setw(2) << blockSize << ": " << std::fixed << std::setprecision(2) << nsPerSample
                  << " ns/amostra (stereo)\n";
    };

    std::cout << "\nBenchmark simples por bloco:\n";
    runBlockBenchmark(16u, 3000u);
    runBlockBenchmark(32u, 3000u);
    runBlockBenchmark(64u, 3000u);

    return 0;
}
