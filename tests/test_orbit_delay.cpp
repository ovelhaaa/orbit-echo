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
    stereo.setMix(-1.0f);
    stereo.setToneHz(50000.0f);
    stereo.setSmearAmount(-10.0f);
    stereo.setStereoSpread(1.0e9f);
    stereo.setOffsetSamples(-1.0e9f);
    stereo.setTempoBpm(2000.0f);
    stereo.setNoteDivision(10.0f);
    stereo.setReadMode(OrbitDelayCore::ReadMode::AccidentalReverse);

    for (int block = 0; block < 8; ++block) {
        stereo.processStereo(inL.data(), inR.data(), outL.data(), outR.data(), static_cast<uint32_t>(inL.size()));
        if (!finiteBuffer(outL) || !finiteBuffer(outR)) {
            return fail("stereo processing should remain finite under clamped extremes");
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
        stereo.setMix(t);
        stereo.setInputGain(0.25f + 3.5f * t);
        stereo.setOutputGain(0.25f + 3.5f * (1.0f - t));
        stereo.setToneHz(300.0f + 11700.0f * t);
        stereo.setSmearAmount(t);
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
