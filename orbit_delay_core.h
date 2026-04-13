#pragma once

#include <cstdint>

#include "dsp_delay_line.h"
#include "dsp_diffuser.h"
#include "dsp_filters.h"

namespace orbit::dsp {

class OrbitDelayCore {
public:
    void reset(float sampleRate);
    bool attachBuffers(float* leftBuffer, uint32_t leftSize, float* rightBuffer = nullptr, uint32_t rightSize = 0);

    void setSampleRate(float sr);
    void setOrbit(float value);
    void setOffsetSamples(float value);
    void setStereoSpread(float value);
    void setFeedback(float value);
    void setMix(float value);
    void setInputGain(float value);
    void setOutputGain(float value);
    void setLowpassCutoffHz(float value);
    void setDiffusion(float value);
    void setDiffuserStages(uint32_t count);
    void setDcBlockEnabled(bool enabled);

    float processSampleMono(float input);
    void processSampleStereo(float inL, float inR, float& outL, float& outR);

    void processMono(const float* input, float* output, uint32_t numSamples);
    void processStereo(const float* inputL, const float* inputR, float* outputL, float* outputR, uint32_t numSamples);

private:
    void syncDspParams();
    static float sanitizeFinite(float value, float fallback);
    float dryPassThrough(float input) const;

    static constexpr float kFallbackSampleRate = 48000.0f;
    static constexpr uint32_t kMinUsefulDelaySize = 4u;
    static constexpr float kStereoSpreadMax = 20000.0f;

    float processChannel(float input, DelayLine& delay, OnePoleLowpass& lp, DCBlocker& dc, AllpassDiffuser& diffuser, float spread);

    float sampleRate_ = kFallbackSampleRate;
    float orbit_ = 0.5f;
    float offsetSamples_ = 1200.0f;
    float stereoSpread_ = 0.0f;
    float feedback_ = 0.35f;
    float mix_ = 0.35f;
    float toneHz_ = 8000.0f;
    float smear_ = 0.0f;
    uint32_t diffuserStages_ = 2u;
    float inputGain_ = 1.0f;
    float outputGain_ = 1.0f;
    bool dcBlockEnabled_ = false;
    bool initialized_ = false;

    bool sampleRateDirty_ = true;
    bool lowpassDirty_ = true;
    bool diffuserDirty_ = true;

    DelayLine delayL_;
    DelayLine delayR_;

    OnePoleLowpass lowpassL_;
    OnePoleLowpass lowpassR_;

    DCBlocker dcL_;
    DCBlocker dcR_;

    AllpassDiffuser diffuserL_;
    AllpassDiffuser diffuserR_;
};

} // namespace orbit::dsp
