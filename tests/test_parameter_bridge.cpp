#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include "parameter_bridge.h"

namespace {

int fail(const char* msg) {
    std::cerr << "FAIL: " << msg << '\n';
    return 1;
}

} // namespace

int main() {
    using orbit::embedded::AudioParams;
    using orbit::embedded::ParameterBridge;

    ParameterBridge bridge;
    std::atomic<bool> run{true};
    std::atomic<uint32_t> published{0};

    std::thread producer([&]() {
        AudioParams params;
        uint32_t seq = 0;
        while (run.load(std::memory_order_relaxed)) {
            params.orbit = 0.25f + 0.001f * static_cast<float>(seq % 2000u);
            params.offsetSamples = static_cast<float>(seq);
            params.stereoSpread = static_cast<float>(seq) * 0.25f;
            params.feedback = 0.1f + static_cast<float>(seq % 85u) / 100.0f;
            params.mix = static_cast<float>(seq % 100u) / 100.0f;
            params.inputGain = 0.5f + static_cast<float>(seq % 10u) * 0.1f;
            params.outputGain = 0.25f + static_cast<float>(seq % 20u) * 0.05f;
            params.toneHz = 300.0f + static_cast<float>(seq % 11700u);
            params.smearAmount = static_cast<float>(seq % 100u) / 100.0f;
            params.diffuserStages = seq % 5u;
            params.dcBlockEnabled = ((seq & 1u) == 0u);

            bridge.publish(params);
            published.store(seq, std::memory_order_relaxed);
            ++seq;
        }
    });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    uint32_t consumedCount = 0;
    uint32_t monotonicCheck = 0;

    while (std::chrono::steady_clock::now() < deadline) {
        AudioParams snapshot;
        if (!bridge.consumeIfUpdated(snapshot)) {
            continue;
        }

        ++consumedCount;
        const uint32_t seq = static_cast<uint32_t>(snapshot.offsetSamples);
        if (seq < monotonicCheck) {
            run.store(false, std::memory_order_relaxed);
            producer.join();
            return fail("consumed sequence moved backwards");
        }
        monotonicCheck = seq;

        const float expectedSpread = static_cast<float>(seq) * 0.25f;
        if (snapshot.stereoSpread != expectedSpread) {
            run.store(false, std::memory_order_relaxed);
            producer.join();
            return fail("detected torn snapshot during concurrent publish");
        }

        const bool expectedDcBlock = ((seq & 1u) == 0u);
        if (snapshot.dcBlockEnabled != expectedDcBlock) {
            run.store(false, std::memory_order_relaxed);
            producer.join();
            return fail("bool field was inconsistent with snapshot sequence");
        }
    }

    run.store(false, std::memory_order_relaxed);
    producer.join();

    if (consumedCount == 0u || published.load(std::memory_order_relaxed) == 0u) {
        return fail("bridge did not publish/consume under load");
    }

    std::cout << "test_parameter_bridge: OK\n";
    return 0;
}
