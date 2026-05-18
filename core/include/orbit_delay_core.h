#pragma once

#include <atomic>
#include <cstdint>

#include "core/include/dsp_delay_line.h"
#include "core/include/dsp_diffuser.h"
#include "core/include/dsp_filters.h"
#include "core/include/dsp_modulation.h"
#include "core/include/dsp_nonlinear.h"
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
    void setLfoRateHz(float value);
    void setLfoDepthSamples(float value);
    void setLfoStereoPhaseOffset(float value);
    void setFeedback(float value);
    void setFeedbackDrive(float value);
    void setFeedbackNonlinearAmount(float value);
    void setFeedbackCompThreshold(float value);
    void setMix(float value);
    void setInputGain(float value);
    void setOutputGain(float value);
    void setToneHz(float value);
    void setSmearAmount(float value);
    void setShimmerMode(bool enabled);
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
        float lfoRateHz = 0.25f;
        float lfoDepthSamples = 0.0f;
        float feedback = 0.35f;
        float feedbackDrive = 1.0f;
        float feedbackNonlinearAmount = 0.0f;
        float feedbackCompThreshold = 1.0f;
        float mix = 0.35f;
        float inputGain = 1.0f;
        float outputGain = 1.0f;
    };

    void applyPendingParamsIfNeeded();
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
    static constexpr float kLfoRateMaxHz = 20.0f;
    static constexpr float kLfoDepthMaxSamples = 20000.0f;
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
    // CPU guideline for MCU use: keep cadence at >=16 samples and keep all smoother
    // math linear; nonlinear envelope coefficients are precomputed outside the hot path.
    static constexpr float kSmoothMixMs = 20.0f;
    static constexpr float kSmoothFeedbackMs = 20.0f;
    static constexpr float kSmoothFeedbackNonlinearMs = 20.0f;
    static constexpr float kSmoothToneMs = 50.0f;
    static constexpr float kSmoothOrbitMs = 35.0f;
    static constexpr float kSmoothOffsetMs = 25.0f;
    static constexpr float kSmoothTempoDelayMs = 25.0f;
    static constexpr float kSmoothSmearMs = 35.0f;
    static constexpr float kSmoothStereoSpreadMs = 8.0f;
    static constexpr float kSmoothLfoRateMs = 50.0f;
    static constexpr float kSmoothLfoDepthMs = 25.0f;
    static constexpr float kSmoothGainMs = 15.0f;

    float processChannel(float input,
                         DelayLine& delay,
                         BiquadLowpass& lp,
                         DCBlocker& dc,
                         AllpassDiffuser& diffuser,
                         EnvelopeFollowerLimiter& feedbackLimiter,
                         const SmoothedParams& params,
                         float spread);
    float processChannelFast(float input,
                             DelayLine& delay,
                             BiquadLowpass& lp,
                             DCBlocker& dc,
                             AllpassDiffuser& diffuser,
                             EnvelopeFollowerLimiter& feedbackLimiter,
                             const SmoothedParams& params,
                             float spread,
                             float lfoSamples,
                             float delaySize,
                             float invDelaySize);
    bool advanceCadence();
    float advanceLfo(ParabolicLfo& lfo, const SmoothedParams& params, float phaseOffset = 0.0f);

    float sampleRate_ = kFallbackSampleRate;
    float orbit_ = 0.5f;
    float offsetSamples_ = 1200.0f;
    float tempoBpm_ = 120.0f;
    float noteDivision_ = 1.0f;
    float tempoDelaySamples_ = 24000.0f;
    float stereoSpread_ = 0.0f;
    float lfoRateHz_ = 0.25f;
    float lfoDepthSamples_ = 0.0f;
    float lfoStereoPhaseOffset_ = 0.25f;
    float feedback_ = 0.35f;
    float feedbackDrive_ = 1.0f;
    float feedbackNonlinearAmount_ = 0.0f;
    float feedbackCompThreshold_ = 1.0f;
    float mix_ = 0.35f;
    float toneHz_ = 8000.0f;
    float smear_ = 0.0f;
    uint32_t diffuserStages_ = 2u;
    float inputGain_ = 1.0f;
    float outputGain_ = 1.0f;
    bool dcBlockEnabled_ = false;
    bool shimmerModeEnabled_ = true;
    ReadMode readMode_ = ReadMode::Orbit;
    FeedbackPreset feedbackPreset_ = FeedbackPreset::Default;
    bool initialized_ = false;
    uint32_t appliedParamVersion_ = 0u;

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
    LinearSmoother feedbackDriveSm_;
    LinearSmoother feedbackNonlinearAmountSm_;
    LinearSmoother feedbackCompThresholdSm_;
    LinearSmoother toneSm_;
    LinearSmoother orbitSm_;
    LinearSmoother offsetSm_;
    LinearSmoother tempoDelaySm_;
    LinearSmoother smearSm_;
    LinearSmoother stereoSpreadSm_;
    LinearSmoother lfoRateSm_;
    LinearSmoother lfoDepthSm_;
    LinearSmoother inputGainSm_;
    LinearSmoother outputGainSm_;

    std::atomic<uint32_t> pendingParamVersion_{1u};
    std::atomic<float> pendingSampleRate_{kFallbackSampleRate};
    std::atomic<float> pendingOrbit_{0.5f};
    std::atomic<float> pendingOffsetSamples_{1200.0f};
    std::atomic<float> pendingTempoBpm_{120.0f};
    std::atomic<float> pendingNoteDivision_{1.0f};
    std::atomic<float> pendingStereoSpread_{0.0f};
    std::atomic<float> pendingLfoRateHz_{0.25f};
    std::atomic<float> pendingLfoDepthSamples_{0.0f};
    std::atomic<float> pendingLfoStereoPhaseOffset_{0.25f};
    std::atomic<float> pendingFeedback_{0.35f};
    std::atomic<float> pendingFeedbackDrive_{1.0f};
    std::atomic<float> pendingFeedbackNonlinearAmount_{0.0f};
    std::atomic<float> pendingFeedbackCompThreshold_{1.0f};
    std::atomic<float> pendingMix_{0.35f};
    std::atomic<float> pendingInputGain_{1.0f};
    std::atomic<float> pendingOutputGain_{1.0f};
    std::atomic<float> pendingToneHz_{8000.0f};
    std::atomic<float> pendingSmear_{0.0f};
    std::atomic<uint32_t> pendingDiffuserStages_{2u};
    std::atomic<bool> pendingDcBlockEnabled_{false};
    std::atomic<bool> pendingShimmerModeEnabled_{true};
    std::atomic<uint32_t> pendingReadMode_{static_cast<uint32_t>(ReadMode::Orbit)};

    DelayLine delayL_;
    DelayLine delayR_;

    ParabolicLfo lfoL_;
    ParabolicLfo lfoR_;

    BiquadLowpass lowpassL_;
    BiquadLowpass lowpassR_;

    DCBlocker dcL_;
    DCBlocker dcR_;

    AllpassDiffuser diffuserL_;
    AllpassDiffuser diffuserR_;

    EnvelopeFollowerLimiter feedbackLimiterL_;
    EnvelopeFollowerLimiter feedbackLimiterR_;
};

} // namespace orbit::dsp
