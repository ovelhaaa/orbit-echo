#pragma once

#include <cstddef>
#include <cstdint>

namespace orbit::embedded {

enum class AudioSourceType : uint8_t {
    ExternalI2s = 0,
    InternalTest = 1,
};

class AudioSource {
public:
    virtual ~AudioSource() = default;

    virtual void prepare(const int32_t* inInterleaved, size_t frames) = 0;
    virtual void renderFrame(size_t frameIndex, float& outL, float& outR) = 0;
};

class I2sInputSource final : public AudioSource {
public:
    void prepare(const int32_t* inInterleaved, size_t frames) override;
    void renderFrame(size_t frameIndex, float& outL, float& outR) override;

private:
    const int32_t* input_ = nullptr;
    size_t frames_ = 0;
};

class InternalTestSilenceSource final : public AudioSource {
public:
    void prepare(const int32_t* inInterleaved, size_t frames) override;
    void renderFrame(size_t frameIndex, float& outL, float& outR) override;
};

AudioSource& selectAudioSource(AudioSourceType type, I2sInputSource& externalI2s, InternalTestSilenceSource& internalTest);

} // namespace orbit::embedded
