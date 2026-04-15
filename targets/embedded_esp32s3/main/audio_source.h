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
    void renderFrame(size_t frameIndex, float& outL, float& outR) const;

private:
    const int32_t* input_ = nullptr;
};

class InternalTestSilenceSource {
public:
    void prepare(const int32_t* inInterleaved);
    void renderFrame(size_t frameIndex, float& outL, float& outR) const;
};

} // namespace orbit::embedded
