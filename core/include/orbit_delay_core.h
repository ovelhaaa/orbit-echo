#pragma once

#include <cstdint>

#include "core/include/dsp_delay_line.h"
#include "core/include/dsp_diffuser.h"
#include "core/include/dsp_filters.h"
#include "core/include/dsp_smoother.h"

namespace orbit::dsp {

class OrbitDelayCore {
public:
    enum class ReadMode : uint32_t {
        Orbit = 0u,
        AccidentalReverse = 1u,
    };

    void reset(float sampleRate);
    bool attachBuffers(float* leftBuffer, float* rightBuffer, uint32_t size);
    bool attachBufferMono(float* buffer, uint32_t size);

    void setSampleRate(float sr);
    void setOrbit(float value);
    void setOffsetSamples(float value);
    void setTempoBpm(float value);
    void setNoteDivision(float value);
    void setStereoSpread(float value);
    void setFeedback(float value);
    void setMix(float value);
    void setInputGain(float value);
    void setOutputGain(float value);
    void setToneHz(float value);
    void setSmearAmount(float value);
    void setDiffuserStages(uint32_t count);
    void setDcBlockEnabled(bool enabled);
    void setReadMode(ReadMode mode);

    [[deprecated("Use attachBuffers(left, right, size) or attachBufferMono(buffer, size)")]]
    bool attachBuffers(float* leftBuffer, uint32_t leftSize, float* rightBuffer, uint32_t rightSize);
    [[deprecated("Use setToneHz(value)")]]
    void setLowpassCutoffHz(float value);
    [[deprecated("Use setSmearAmount(value)")]]
    void setDiffusion(float value);

    float processSampleMono(float input);
    void processSampleStereo(float inL, float inR, float& outL, float& outR);

    void processMono(const float* input, float* output, uint32_t numSamples);
    void processStereo(const float* inputL, const float* inputR, float* outputL, float* outputR, uint32_t numSamples);

private:
    enum class FeedbackPreset : uint32_t {
        Default = 0u,
        ReverseLegacy = 1u,
    };

    struct SmoothedParams {
        float orbit = 0.5f;
        float offsetSamples = 1200.0f;
        float tempoDelaySamples = 24000.0f;
        float stereoSpread = 0.0f;
        float feedback = 0.35f;
        float mix = 0.35f;
    };

    void syncDspParams();
    void updateSmoothedTargetsIfDirty();
    SmoothedParams advanceSmoothers();
    void maybeApplyLowpassCutoff(float smoothedToneHz);
    void maybeApplyDiffuserAmount(float smoothedSmear);
    float feedbackLowpassQ() const;
    static float sanitizeFinite(float value, float fallback);
    float dryPassThrough(float input) const;

    static constexpr float kFallbackSampleRate = 48000.0f;
    static constexpr uint32_t kMinUsefulDelaySize = 4u;
    static constexpr float kStereoSpreadMax = 20000.0f;
    static constexpr float kMinTempoBpm = 20.0f;
    static constexpr float kMaxTempoBpm = 320.0f;
    static constexpr float kMinNoteDivision = 0.0625f;
    static constexpr float kMaxNoteDivision = 4.0f;
    static constexpr uint32_t kHeavyParamCadenceSamples = 16u;
    static constexpr float kLowpassUpdateDeltaHz = 20.0f;
    static constexpr float kDiffuserUpdateDelta = 0.01f;
    static constexpr float kDefaultFeedbackLowpassQ = 0.707f;
    static constexpr float kReverseLegacyFeedbackLowpassQ = 0.5f;
    static constexpr float kReverseLegacyToneHz = 1800.0f;

    // Default smoothing times tuned for MCU targets:
    // - core modulation params: 8-35 ms (zipper-noise suppression with low CPU).
    // - tone/smear controls: 35-50 ms (less frequent expensive coefficient updates).
    // CPU guideline for MCU use: keep cadence at >=16 samples and avoid >7 smoothers
    // with per-sample transcendental math in the hot path.
    static constexpr float kSmoothMixMs = 20.0f;
    static constexpr float kSmoothFeedbackMs = 20.0f;
    static constexpr float kSmoothToneMs = 50.0f;
    static constexpr float kSmoothOrbitMs = 35.0f;
    static constexpr float kSmoothOffsetMs = 25.0f;
    static constexpr float kSmoothTempoDelayMs = 25.0f;
    static constexpr float kSmoothSmearMs = 35.0f;
    static constexpr float kSmoothStereoSpreadMs = 8.0f;

    float processChannel(float input, DelayLine& delay, BiquadLowpass& lp, DCBlocker& dc, AllpassDiffuser& diffuser,
                         const SmoothedParams& params, float spread);
    float processChannelFast(float input, DelayLine& delay, BiquadLowpass& lp, DCBlocker& dc, AllpassDiffuser& diffuser,
                             const SmoothedParams& params, float spread, float delaySize, float invDelaySize);
    bool advanceCadence();

    float sampleRate_ = kFallbackSampleRate;
    float orbit_ = 0.5f;
    float offsetSamples_ = 1200.0f;
    float tempoBpm_ = 120.0f;
    float noteDivision_ = 1.0f;
    float tempoDelaySamples_ = 24000.0f;
    float stereoSpread_ = 0.0f;
    float feedback_ = 0.35f;
    float mix_ = 0.35f;
    float toneHz_ = 8000.0f;
    float smear_ = 0.0f;
    uint32_t diffuserStages_ = 2u;
    float inputGain_ = 1.0f;
    float outputGain_ = 1.0f;
    bool dcBlockEnabled_ = false;
    ReadMode readMode_ = ReadMode::Orbit;
    FeedbackPreset feedbackPreset_ = FeedbackPreset::Default;
    bool initialized_ = false;

    bool sampleRateDirty_ = true;
    bool lowpassDirty_ = true;
    bool diffuserDirty_ = true;
    bool smoothTargetsDirty_ = true;
    uint32_t heavyParamCadenceCountdown_ = 1u;
    bool heavyParamCadenceHit_ = false;
    float appliedToneHz_ = 8000.0f;
    float appliedSmear_ = 0.0f;

    LinearSmoother mixSm_;
    LinearSmoother feedbackSm_;
    LinearSmoother toneSm_;
    LinearSmoother orbitSm_;
    LinearSmoother offsetSm_;
    LinearSmoother tempoDelaySm_;
    LinearSmoother smearSm_;
    LinearSmoother stereoSpreadSm_;

    DelayLine delayL_;
    DelayLine delayR_;

    BiquadLowpass lowpassL_;
    BiquadLowpass lowpassR_;

    DCBlocker dcL_;
    DCBlocker dcR_;

    AllpassDiffuser diffuserL_;
    AllpassDiffuser diffuserR_;
};

} // namespace orbit::dsp
