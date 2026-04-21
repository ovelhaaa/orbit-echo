#include "audio_source.h"

namespace orbit::embedded {
namespace {
constexpr float kPcm1808Scale24 = 8388608.0f; // 2^23
constexpr float kDcBlockA = 0.995f;

inline float unpackPcm1808SampleToFloat(int32_t raw) {
    // PCM1808 costuma chegar em slot de 32 bits com amostra de 24 bits alinhada no topo.
    // Este packing pode variar por target/driver, por isso está isolado nesta função.
    const int32_t s24 = raw >> 8;
    return static_cast<float>(s24) / kPcm1808Scale24;
}
} // namespace

void I2sInputSource::prepare(const int32_t* inInterleaved) {
    input_ = inInterleaved;
}

void I2sInputSource::renderFrame(size_t frameIndex, float& outL, float& outR) {
    if (!input_) {
        outL = 0.0f;
        outR = 0.0f;
        return;
    }

    const float xL = unpackPcm1808SampleToFloat(input_[frameIndex * 2]);
    const float xR = unpackPcm1808SampleToFloat(input_[frameIndex * 2 + 1]);

    // Remove offset DC residual para reduzir acúmulo em caminhos com delay/feedback.
    const float yL = xL - x1L_ + (kDcBlockA * y1L_);
    const float yR = xR - x1R_ + (kDcBlockA * y1R_);

    x1L_ = xL;
    x1R_ = xR;
    y1L_ = yL;
    y1R_ = yR;

    outL = yL;
    outR = yR;
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
