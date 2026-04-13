#include <cmath>
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

    for (int block = 0; block < 8; ++block) {
        stereo.processStereo(inL.data(), inR.data(), outL.data(), outR.data(), static_cast<uint32_t>(inL.size()));
        if (!finiteBuffer(outL) || !finiteBuffer(outR)) {
            return fail("stereo processing should remain finite under clamped extremes");
        }
    }

    std::cout << "test_orbit_delay: OK\n";
    return 0;
}
