#include <cmath>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

#include "core/include/orbit_delay_core.h"

namespace {

int fail(const char* message) {
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

bool finiteBuffer(const std::vector<float>& data) {
    for (float v : data) {
        if (!std::isfinite(v)) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    using orbit::dsp::OrbitDelayCore;

    OrbitDelayCore core;
    core.reset(48000.0f);

    // Null pointers: não deve crashar e deve manter estado estável.
    core.processMono(nullptr, nullptr, 16u);
    core.processStereo(nullptr, nullptr, nullptr, nullptr, 16u);

    // Buffer mínimo inválido (<4) deve falhar attach e operar em dry passthrough.
    std::vector<float> small(3u, 0.0f);
    if (core.attachBufferMono(small.data(), static_cast<uint32_t>(small.size()))) {
        return fail("attachBufferMono should fail for size < 4");
    }

    const float dry = core.processSampleMono(0.25f);
    if (!std::isfinite(dry) || std::fabs(dry - 0.25f) > 1.0e-6f) {
        return fail("mono sample should pass through when uninitialized");
    }

    // Mono válido.
    std::vector<float> monoDelay(64u, 0.0f);
    if (!core.attachBufferMono(monoDelay.data(), static_cast<uint32_t>(monoDelay.size()))) {
        return fail("attachBufferMono should succeed with size >= 4");
    }

    std::vector<float> monoIn(128u, 0.0f);
    monoIn[0] = 1.0f;
    std::vector<float> monoOut(128u, 0.0f);
    core.processMono(monoIn.data(), monoOut.data(), static_cast<uint32_t>(monoIn.size()));
    if (!finiteBuffer(monoOut)) {
        return fail("mono output should remain finite");
    }

    // Stereo válido.
    OrbitDelayCore stereo;
    stereo.reset(48000.0f);
    std::vector<float> leftDelay(64u, 0.0f);
    std::vector<float> rightDelay(64u, 0.0f);
    if (!stereo.attachBuffers(leftDelay.data(), rightDelay.data(), static_cast<uint32_t>(leftDelay.size()))) {
        return fail("attachBuffers should succeed with valid stereo buffers");
    }

    std::vector<float> inL(128u, 0.0f);
    std::vector<float> inR(128u, 0.0f);
    inL[0] = 1.0f;
    inR[1] = 0.5f;
    std::vector<float> outL(128u, 0.0f);
    std::vector<float> outR(128u, 0.0f);
    stereo.processStereo(inL.data(), inR.data(), outL.data(), outR.data(), static_cast<uint32_t>(inL.size()));

    if (!finiteBuffer(outL) || !finiteBuffer(outR)) {
        return fail("stereo outputs should remain finite");
    }

    // Estabilidade básica com parâmetros extremos (clamps internos).
    stereo.setFeedback(2.0f);
    stereo.setFeedbackDrive(100.0f);
    stereo.setFeedbackNonlinearAmount(2.0f);
    stereo.setFeedbackCompThreshold(-1.0f);
    stereo.setMix(-1.0f);
    stereo.setToneHz(50000.0f);
    stereo.setSmearAmount(-10.0f);
    stereo.setStereoSpread(1.0e9f);
    stereo.setOffsetSamples(-1.0e9f);
    stereo.setTempoBpm(2000.0f);
    stereo.setNoteDivision(10.0f);
    stereo.setLfoRateHz(1000.0f);
    stereo.setLfoDepthSamples(1.0e9f);
    stereo.setLfoStereoPhaseOffset(1.0e30f);
    stereo.setReadMode(OrbitDelayCore::ReadMode::AccidentalReverse);

    for (int block = 0; block < 8; ++block) {
        stereo.processStereo(inL.data(), inR.data(), outL.data(), outR.data(), static_cast<uint32_t>(inL.size()));
        if (!finiteBuffer(outL) || !finiteBuffer(outR)) {
            return fail("stereo processing should remain finite under clamped extremes");
        }
    }


    // LFO estéreo deve avançar uma vez por canal/amostra no processamento em bloco,
    // preservando equivalência com o caminho sample-by-sample.
    OrbitDelayCore blockLfo;
    OrbitDelayCore sampleLfo;
    blockLfo.reset(48000.0f);
    sampleLfo.reset(48000.0f);
    std::vector<float> blockDelayL(256u, 0.0f);
    std::vector<float> blockDelayR(256u, 0.0f);
    std::vector<float> sampleDelayL(256u, 0.0f);
    std::vector<float> sampleDelayR(256u, 0.0f);
    if (!blockLfo.attachBuffers(blockDelayL.data(), blockDelayR.data(), static_cast<uint32_t>(blockDelayL.size())) ||
        !sampleLfo.attachBuffers(sampleDelayL.data(), sampleDelayR.data(), static_cast<uint32_t>(sampleDelayL.size()))) {
        return fail("LFO stereo attach should succeed");
    }
    blockLfo.setOffsetSamples(48.0f);
    sampleLfo.setOffsetSamples(48.0f);
    blockLfo.setMix(1.0f);
    sampleLfo.setMix(1.0f);
    blockLfo.setLfoRateHz(2.0f);
    sampleLfo.setLfoRateHz(2.0f);
    blockLfo.setLfoDepthSamples(8.0f);
    sampleLfo.setLfoDepthSamples(8.0f);
    blockLfo.setLfoStereoPhaseOffset(0.25f);
    sampleLfo.setLfoStereoPhaseOffset(0.25f);

    std::vector<float> lfoInL(96u, 0.0f);
    std::vector<float> lfoInR(96u, 0.0f);
    for (uint32_t i = 0; i < lfoInL.size(); ++i) {
        lfoInL[i] = (i == 0u) ? 1.0f : 0.01f * static_cast<float>(i % 7u);
        lfoInR[i] = (i == 3u) ? 0.75f : -0.01f * static_cast<float>(i % 5u);
    }
    std::vector<float> blockOutL(lfoInL.size(), 0.0f);
    std::vector<float> blockOutR(lfoInR.size(), 0.0f);
    std::vector<float> sampleOutL(lfoInL.size(), 0.0f);
    std::vector<float> sampleOutR(lfoInR.size(), 0.0f);
    blockLfo.processStereo(lfoInL.data(), lfoInR.data(), blockOutL.data(), blockOutR.data(), static_cast<uint32_t>(lfoInL.size()));
    for (uint32_t i = 0; i < lfoInL.size(); ++i) {
        sampleLfo.processSampleStereo(lfoInL[i], lfoInR[i], sampleOutL[i], sampleOutR[i]);
    }
    for (uint32_t i = 0; i < lfoInL.size(); ++i) {
        if (std::fabs(blockOutL[i] - sampleOutL[i]) > 1.0e-6f ||
            std::fabs(blockOutR[i] - sampleOutR[i]) > 1.0e-6f) {
            return fail("block stereo LFO should match sample-by-sample LFO advancement");
        }
    }

    // Sweep contínuo de parâmetros (simula drag/knob) sem NaN e sem regressão grosseira de performance.
    constexpr uint32_t kSweepBlock = 128u;
    constexpr uint32_t kSweepBlocks = 800u;
    std::vector<float> sweepInL(kSweepBlock, 0.0f);
    std::vector<float> sweepInR(kSweepBlock, 0.0f);
    std::vector<float> sweepOutL(kSweepBlock, 0.0f);
    std::vector<float> sweepOutR(kSweepBlock, 0.0f);
    for (uint32_t i = 0; i < kSweepBlock; ++i) {
        const float phase = 2.0f * 3.14159265359f * static_cast<float>(i) / static_cast<float>(kSweepBlock);
        sweepInL[i] = std::sin(phase) * 0.25f;
        sweepInR[i] = std::cos(phase) * 0.25f;
    }

    const auto t0 = std::chrono::steady_clock::now();
    for (uint32_t block = 0; block < kSweepBlocks; ++block) {
        const float t = static_cast<float>(block) / static_cast<float>(kSweepBlocks - 1u);
        stereo.setOrbit(0.25f + 2.75f * t);
        stereo.setOffsetSamples(-24000.0f + 48000.0f * t);
        stereo.setStereoSpread(10.0f + 5000.0f * t);
        stereo.setFeedback(0.05f + 0.85f * t);
        stereo.setFeedbackDrive(1.0f + 7.0f * t);
        stereo.setFeedbackNonlinearAmount(t);
        stereo.setFeedbackCompThreshold(0.1f + 1.9f * (1.0f - t));
        stereo.setMix(t);
        stereo.setInputGain(0.25f + 3.5f * t);
        stereo.setOutputGain(0.25f + 3.5f * (1.0f - t));
        stereo.setToneHz(300.0f + 11700.0f * t);
        stereo.setSmearAmount(t);
        stereo.setLfoRateHz(0.1f + 4.0f * t);
        stereo.setLfoDepthSamples(250.0f * t);
        stereo.processStereo(sweepInL.data(), sweepInR.data(), sweepOutL.data(), sweepOutR.data(), kSweepBlock);
        if (!finiteBuffer(sweepOutL) || !finiteBuffer(sweepOutR)) {
            return fail("parameter sweep produced non-finite output");
        }
    }
    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    if (elapsedMs > 1500) {
        return fail("parameter sweep processing too slow");
    }

    std::cout << "test_orbit_delay: OK\n";
    return 0;
}
