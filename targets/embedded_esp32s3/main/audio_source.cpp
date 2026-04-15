#include "audio_source.h"

namespace orbit::embedded {

void I2sInputSource::prepare(const int32_t* inInterleaved) {
    input_ = inInterleaved;
}

void I2sInputSource::renderFrame(size_t frameIndex, float& outL, float& outR) const {
    if (!input_) {
        outL = 0.0f;
        outR = 0.0f;
        return;
    }

    outL = static_cast<float>(input_[frameIndex * 2]) / 2147483648.0f;
    outR = static_cast<float>(input_[frameIndex * 2 + 1]) / 2147483648.0f;
}

void InternalTestTriangleSource::setSampleRate(float sampleRateHz) {
    if (sampleRateHz > 1000.0f) {
        sampleRateHz_ = sampleRateHz;
    }
}

void InternalTestTriangleSource::setFrequencyHz(float frequencyHz) {
    if (frequencyHz > 0.0f) {
        frequencyHz_ = frequencyHz;
    }
}

void InternalTestTriangleSource::setLevel(float level) {
    if (level < 0.0f) {
        level_ = 0.0f;
        return;
    }
    if (level > 1.0f) {
        level_ = 1.0f;
        return;
    }
    level_ = level;
}

void InternalTestTriangleSource::prepare(const int32_t* inInterleaved) {
    (void)inInterleaved;
}

void InternalTestTriangleSource::renderFrame(size_t frameIndex, float& outL, float& outR) {
    (void)frameIndex;

    const float tri = 1.0f - 4.0f * __builtin_fabsf(phase_ - 0.5f);
    const float sample = tri * level_;

    outL = sample;
    outR = sample;

    phase_ += frequencyHz_ / sampleRateHz_;
    if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
    }
}

} // namespace orbit::embedded
