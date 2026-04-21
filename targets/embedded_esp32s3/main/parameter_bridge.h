#pragma once

#include <atomic>
#include <cstdint>

#include "audio_source.h"

namespace orbit::embedded {

struct AudioParams {
    enum class ReadMode : uint32_t {
        Accidental = 0u,
        Orbit = 1u,
    };

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
    ReadMode readMode = ReadMode::Accidental;
    AudioSourceType sourceType = AudioSourceType::InternalTest;
};

class ParameterBridge {
public:
    ParameterBridge();

    void publish(const AudioParams& params);
    bool consumeIfUpdated(AudioParams& outParams);

private:
    std::atomic<uint32_t> publishedVersion_;
    uint32_t lastConsumedVersion_;

    std::atomic<float> orbit_;
    std::atomic<float> offsetSamples_;
    std::atomic<float> stereoSpread_;
    std::atomic<float> feedback_;
    std::atomic<float> mix_;
    std::atomic<float> inputGain_;
    std::atomic<float> outputGain_;
    std::atomic<float> toneHz_;
    std::atomic<float> smearAmount_;
    std::atomic<uint32_t> diffuserStages_;
    std::atomic<bool> dcBlockEnabled_;
    std::atomic<AudioParams::ReadMode> readMode_;
    std::atomic<AudioSourceType> sourceType_;
};

} // namespace orbit::embedded