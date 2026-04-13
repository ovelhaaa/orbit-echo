#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "core/include/orbit_delay_core.h"

int main() {
    namespace fs = std::filesystem;
    using orbit::dsp::OrbitDelayCore;

    constexpr uint32_t kNumSamples = 256u;
    constexpr uint32_t kDelaySize = 512u;

    OrbitDelayCore core;
    core.reset(48000.0f);
    core.setOrbit(0.75f);
    core.setOffsetSamples(120.0f);
    core.setStereoSpread(12.0f);
    core.setFeedback(0.42f);
    core.setMix(0.35f);
    core.setToneHz(6500.0f);
    core.setSmearAmount(0.15f);
    core.setDcBlockEnabled(true);

    std::vector<float> delayL(kDelaySize, 0.0f);
    std::vector<float> delayR(kDelaySize, 0.0f);
    if (!core.attachBuffers(delayL.data(), delayR.data(), kDelaySize)) {
        std::cerr << "Could not attach delay buffers\n";
        return 1;
    }

    std::vector<float> inL(kNumSamples, 0.0f);
    std::vector<float> inR(kNumSamples, 0.0f);
    for (uint32_t i = 0; i < kNumSamples; ++i) {
        inL[i] = ((i % 23u) == 0u) ? 1.0f : 0.0f;
        inR[i] = ((i % 29u) == 0u) ? 0.7f : 0.0f;
    }

    std::vector<float> outL(kNumSamples, 0.0f);
    std::vector<float> outR(kNumSamples, 0.0f);
    core.processStereo(inL.data(), inR.data(), outL.data(), outR.data(), kNumSamples);

    const fs::path goldenDir = fs::path("tests") / "golden";
    fs::create_directories(goldenDir);
    const fs::path goldenFile = goldenDir / "orbit_delay_reference.txt";

    std::ofstream out(goldenFile);
    if (!out.is_open()) {
        std::cerr << "Could not open golden output file: " << goldenFile << "\n";
        return 1;
    }

    out << std::fixed << std::setprecision(9);
    out << "# orbit_delay deterministic golden render\n";
    out << "# sample_index outL outR\n";
    for (uint32_t i = 0; i < kNumSamples; ++i) {
        out << i << ' ' << outL[i] << ' ' << outR[i] << '\n';
    }

    std::cout << "Wrote golden reference to " << goldenFile << "\n";
    return 0;
}
