#pragma once

#include <atomic>
#include <cstdint>

namespace orbit::embedded {

struct AudioParams {
    float orbit = 0.5f;
    float offsetSamples = 1200.0f;
    float stereoSpread = 0.0f;
    float feedback = 0.35f;
    float mix = 0.35f;
    float inputGain = 1.0f;
    float outputGain = 1.0f;
    float toneHz = 8000.0f;
    float smearAmount = 0.0f;
    uint32_t diffuserStages = 2;
    bool dcBlockEnabled = true;
};

class ParameterBridge {
public:
    ParameterBridge();

    void publish(const AudioParams& params);
    bool consumeIfUpdated(AudioParams& outParams);

private:
    AudioParams slots_[2];
    std::atomic<uint32_t> publishedVersion_;
    uint32_t lastConsumedVersion_;
};

} // namespace orbit::embedded
