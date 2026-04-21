#pragma once

#include <cstddef>
#include <cstdint>

namespace orbit::embedded {

enum class AudioSourceType : uint8_t {
    ExternalI2s = 0,
    InternalTest = 1,
};

class I2sInputSource {
public:
    void prepare(const int32_t* inInterleaved);
    void renderFrame(size_t frameIndex, float& outL, float& outR);

private:
    const int32_t* input_ = nullptr;
    float x1L_ = 0.0f;
    float x1R_ = 0.0f;
    float y1L_ = 0.0f;
    float y1R_ = 0.0f;
};

class InternalTestTriangleSource {
public:
    void setSampleRate(float sampleRateHz);
    void setFrequencyHz(float frequencyHz);
    void setLevel(float level);

    void prepare(const int32_t* inInterleaved);
    void renderFrame(size_t frameIndex, float& outL, float& outR);

private:
    float sampleRateHz_ = 48000.0f;
    float frequencyHz_ = 220.0f;
    float level_ = 0.15f;
    float phase_ = 0.0f;
};

} // namespace orbit::embedded
