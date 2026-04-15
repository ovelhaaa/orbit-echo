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

void InternalTestSilenceSource::prepare(const int32_t* inInterleaved) {
    (void)inInterleaved;
}

void InternalTestSilenceSource::renderFrame(size_t frameIndex, float& outL, float& outR) const {
    (void)frameIndex;
    outL = 0.0f;
    outR = 0.0f;
}

} // namespace orbit::embedded
