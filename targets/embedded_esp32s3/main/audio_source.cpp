#include "audio_source.h"

namespace orbit::embedded {

void I2sInputSource::prepare(const int32_t* inInterleaved, size_t frames) {
    input_ = inInterleaved;
    frames_ = frames;
}

void I2sInputSource::renderFrame(size_t frameIndex, float& outL, float& outR) {
    if (!input_ || frameIndex >= frames_) {
        outL = 0.0f;
        outR = 0.0f;
        return;
    }

    outL = static_cast<float>(input_[frameIndex * 2]) / 2147483648.0f;
    outR = static_cast<float>(input_[frameIndex * 2 + 1]) / 2147483648.0f;
}

void InternalTestSilenceSource::prepare(const int32_t* inInterleaved, size_t frames) {
    (void)inInterleaved;
    (void)frames;
}

void InternalTestSilenceSource::renderFrame(size_t frameIndex, float& outL, float& outR) {
    (void)frameIndex;
    outL = 0.0f;
    outR = 0.0f;
}

AudioSource& selectAudioSource(AudioSourceType type,
                               I2sInputSource& externalI2s,
                               InternalTestSilenceSource& internalTest) {
    if (type == AudioSourceType::InternalTest) {
        return internalTest;
    }
    return externalI2s;
}

} // namespace orbit::embedded
