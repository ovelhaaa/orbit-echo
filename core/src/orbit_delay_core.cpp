#include "core/include/orbit_delay_core.h"

#include <cmath>

#include "core/include/dsp_utils.h"

namespace orbit::dsp {

namespace {
bool attachBuffersImpl(DelayLine& delayL,
                       DelayLine& delayR,
                       bool& initialized,
                       float* leftBuffer,
                       uint32_t leftSize,
                       float* rightBuffer,
                       uint32_t rightSize,
                       uint32_t minUsefulDelaySize) {
    if (leftBuffer == nullptr || leftSize < minUsefulDelaySize) {
        initialized = false;
        return false;
    }

    if (rightBuffer != nullptr && rightSize < minUsefulDelaySize) {
        initialized = false;
        return false;
    }

    const bool leftOk = delayL.attach(leftBuffer, leftSize);
    const bool rightRequested = (rightBuffer != nullptr);
    const bool rightOk = rightRequested ? delayR.attach(rightBuffer, rightSize) : false;

    if (!rightRequested) {
        delayR.buffer = nullptr;
        delayR.size = 0;
        delayR.writePos = 0;
    }

    initialized = leftOk && (!rightRequested || rightOk);
    return initialized;
}
} // namespace

float OrbitDelayCore::sanitizeFinite(float value, float fallback) {
    return isFiniteSafe(value) ? value : fallback;
}

float OrbitDelayCore::dryPassThrough(float input) const {
    return sanitizeFinite(input * outputGain_, 0.0f);
}

namespace {
float computeTempoDelaySamples(float sampleRate, float tempoBpm, float noteDivision) {
    const float beatMs = 60000.0f / tempoBpm;
    const float delayMs = beatMs * noteDivision;
    return (delayMs * sampleRate) / 1000.0f;
}
} // namespace

void OrbitDelayCore::applyPendingParamsIfNeeded() {
    const uint32_t version = pendingParamVersion_.load(std::memory_order_acquire);
    if (version == appliedParamVersion_) {
        return;
    }

    const float nextSampleRate = pendingSampleRate_.load(std::memory_order_relaxed);
    const float nextOrbit = pendingOrbit_.load(std::memory_order_relaxed);
    const float nextOffset = pendingOffsetSamples_.load(std::memory_order_relaxed);
    const float nextTempoBpm = pendingTempoBpm_.load(std::memory_order_relaxed);
    const float nextNoteDivision = pendingNoteDivision_.load(std::memory_order_relaxed);
    const float nextStereoSpread = pendingStereoSpread_.load(std::memory_order_relaxed);
    const float nextFeedback = pendingFeedback_.load(std::memory_order_relaxed);
    const float nextMix = pendingMix_.load(std::memory_order_relaxed);
    const float nextInputGain = pendingInputGain_.load(std::memory_order_relaxed);
    const float nextOutputGain = pendingOutputGain_.load(std::memory_order_relaxed);
    const float nextToneHz = pendingToneHz_.load(std::memory_order_relaxed);
    const float nextSmear = pendingSmear_.load(std::memory_order_relaxed);
    const uint32_t nextDiffuserStages = pendingDiffuserStages_.load(std::memory_order_relaxed);
    const bool nextDcBlock = pendingDcBlockEnabled_.load(std::memory_order_relaxed);
    const bool nextShimmer = pendingShimmerModeEnabled_.load(std::memory_order_relaxed);
    const ReadMode nextReadMode = static_cast<ReadMode>(pendingReadMode_.load(std::memory_order_relaxed));

    const float clampedSampleRate = clampf(sanitizeFinite(nextSampleRate, kFallbackSampleRate), 1.0f, 384000.0f);
    if (sampleRate_ != clampedSampleRate) {
        sampleRate_ = clampedSampleRate;
        sampleRateDirty_ = true;
    }

    orbit_ = clampf(sanitizeFinite(nextOrbit, orbit_), 0.25f, 3.0f);
    offsetSamples_ = clampf(sanitizeFinite(nextOffset, offsetSamples_), -200000.0f, 200000.0f);
    tempoBpm_ = clampf(sanitizeFinite(nextTempoBpm, tempoBpm_), kMinTempoBpm, kMaxTempoBpm);
    noteDivision_ = clampf(sanitizeFinite(nextNoteDivision, noteDivision_), kMinNoteDivision, kMaxNoteDivision);
    stereoSpread_ = clampf(sanitizeFinite(nextStereoSpread, stereoSpread_), 0.0f, kStereoSpreadMax);
    feedback_ = clampf(sanitizeFinite(nextFeedback, feedback_), 0.0f, 0.95f);
    mix_ = clampf(sanitizeFinite(nextMix, mix_), 0.0f, 1.0f);
    inputGain_ = clampf(sanitizeFinite(nextInputGain, inputGain_), 0.0f, 4.0f);
    outputGain_ = clampf(sanitizeFinite(nextOutputGain, outputGain_), 0.0f, 4.0f);
    toneHz_ = clampf(sanitizeFinite(nextToneHz, toneHz_), 300.0f, 12000.0f);
    smear_ = clampf(sanitizeFinite(nextSmear, smear_), 0.0f, 1.0f);
    diffuserStages_ = (nextDiffuserStages > AllpassDiffuser::kMaxStages) ? AllpassDiffuser::kMaxStages : nextDiffuserStages;
    dcBlockEnabled_ = nextDcBlock;
    if (shimmerModeEnabled_ != nextShimmer) {
        shimmerModeEnabled_ = nextShimmer;
        diffuserL_.reset();
        diffuserR_.reset();
    }

    if (readMode_ != nextReadMode) {
        readMode_ = nextReadMode;
        if (readMode_ == ReadMode::AccidentalReverse) {
            feedbackPreset_ = FeedbackPreset::ReverseLegacy;
            toneHz_ = kReverseLegacyToneHz;
            pendingToneHz_.store(toneHz_, std::memory_order_relaxed);
        } else {
            feedbackPreset_ = FeedbackPreset::Default;
        }
        lowpassDirty_ = true;
    }

    tempoDelaySamples_ = computeTempoDelaySamples(sampleRate_, tempoBpm_, noteDivision_);
    smoothTargetsDirty_ = true;
    diffuserDirty_ = true;
    appliedParamVersion_ = version;
}

void OrbitDelayCore::syncDspParams() {
    applyPendingParamsIfNeeded();
    if (sampleRateDirty_) {
        lowpassL_.setSampleRate(sampleRate_);
        lowpassR_.setSampleRate(sampleRate_);
        dcL_.setSampleRate(sampleRate_);
        dcR_.setSampleRate(sampleRate_);
        mixSm_.configure(sampleRate_, kSmoothMixMs);
        feedbackSm_.configure(sampleRate_, kSmoothFeedbackMs);
        toneSm_.configure(sampleRate_, kSmoothToneMs);
        orbitSm_.configure(sampleRate_, kSmoothOrbitMs);
        offsetSm_.configure(sampleRate_, kSmoothOffsetMs);
        tempoDelaySm_.configure(sampleRate_, kSmoothTempoDelayMs);
        smearSm_.configure(sampleRate_, kSmoothSmearMs);
        stereoSpreadSm_.configure(sampleRate_, kSmoothStereoSpreadMs);
        inputGainSm_.configure(sampleRate_, kSmoothGainMs);
        outputGainSm_.configure(sampleRate_, kSmoothGainMs);
        tempoDelaySamples_ = computeTempoDelaySamples(sampleRate_, tempoBpm_, noteDivision_);
        smoothTargetsDirty_ = true;
        sampleRateDirty_ = false;
        lowpassDirty_ = true;
    }

    if (lowpassDirty_) {
        const float nyquistLimited = clampf(toneHz_, 300.0f, clampf(0.49f * sampleRate_, 300.0f, 12000.0f));
        const float lowpassQ = feedbackLowpassQ();
        lowpassL_.setParams(nyquistLimited, lowpassQ);
        lowpassR_.setParams(nyquistLimited, lowpassQ);
        lowpassDirty_ = false;
    }

    if (diffuserDirty_) {
        diffuserL_.setStageCount(diffuserStages_);
        diffuserR_.setStageCount(diffuserStages_);
        appliedSmear_ = smearSm_.current;
        diffuserL_.setAmount(appliedSmear_);
        diffuserR_.setAmount(appliedSmear_);
        diffuserDirty_ = false;
    }
}

void OrbitDelayCore::updateSmoothedTargetsIfDirty() {
    if (!smoothTargetsDirty_) {
        return;
    }
    orbitSm_.setTarget(orbit_);
    offsetSm_.setTarget(offsetSamples_);
    tempoDelaySm_.setTarget(tempoDelaySamples_);
    stereoSpreadSm_.setTarget(stereoSpread_);
    feedbackSm_.setTarget(feedback_);
    mixSm_.setTarget(mix_);
    inputGainSm_.setTarget(inputGain_);
    outputGainSm_.setTarget(outputGain_);
    toneSm_.setTarget(toneHz_);
    smearSm_.setTarget(smear_);
    smoothTargetsDirty_ = false;
}

OrbitDelayCore::SmoothedParams OrbitDelayCore::advanceSmoothers() {
    SmoothedParams params;
    params.orbit = orbitSm_.next();
    params.offsetSamples = offsetSm_.next();
    params.tempoDelaySamples = tempoDelaySm_.next();
    params.stereoSpread = stereoSpreadSm_.next();
    params.feedback = feedbackSm_.next();
    params.mix = mixSm_.next();
    params.inputGain = inputGainSm_.next();
    params.outputGain = outputGainSm_.next();

    const float smoothedToneHz = toneSm_.next();
    const float smoothedSmear = smearSm_.next();
    heavyParamCadenceHit_ = advanceCadence();

    maybeApplyLowpassCutoff(smoothedToneHz);
    maybeApplyDiffuserAmount(smoothedSmear);
    return params;
}

bool OrbitDelayCore::advanceCadence() {
    if (heavyParamCadenceCountdown_ <= 1u) {
        heavyParamCadenceCountdown_ = kHeavyParamCadenceSamples;
        return true;
    }
    --heavyParamCadenceCountdown_;
    return false;
}

void OrbitDelayCore::maybeApplyLowpassCutoff(float smoothedToneHz) {
    const float clampedToneHz = clampf(smoothedToneHz, 300.0f, clampf(0.49f * sampleRate_, 300.0f, 12000.0f));
    const float delta = std::fabs(clampedToneHz - appliedToneHz_);
    if (lowpassDirty_ || heavyParamCadenceHit_ || delta >= kLowpassUpdateDeltaHz) {
        const float lowpassQ = feedbackLowpassQ();
        lowpassL_.setParams(clampedToneHz, lowpassQ);
        lowpassR_.setParams(clampedToneHz, lowpassQ);
        appliedToneHz_ = clampedToneHz;
        lowpassDirty_ = false;
    }
}

void OrbitDelayCore::maybeApplyDiffuserAmount(float smoothedSmear) {
    const float delta = std::fabs(smoothedSmear - appliedSmear_);
    if (diffuserDirty_ || heavyParamCadenceHit_ || delta >= kDiffuserUpdateDelta) {
        const float clamped = clampf(smoothedSmear, 0.0f, 1.0f);
        diffuserL_.setAmount(clamped);
        diffuserR_.setAmount(clamped);
        appliedSmear_ = clamped;
        diffuserDirty_ = false;
    }
}

float OrbitDelayCore::feedbackLowpassQ() const {
    if (feedbackPreset_ == FeedbackPreset::ReverseLegacy) {
        return kReverseLegacyFeedbackLowpassQ;
    }
    return kDefaultFeedbackLowpassQ;
}

void OrbitDelayCore::reset(float sampleRate) {
    setSampleRate(sampleRate);
    syncDspParams();
    orbitSm_.reset(orbit_);
    offsetSm_.reset(offsetSamples_);
    tempoDelaySamples_ = computeTempoDelaySamples(sampleRate_, tempoBpm_, noteDivision_);
    tempoDelaySm_.reset(tempoDelaySamples_);
    stereoSpreadSm_.reset(stereoSpread_);
    feedbackSm_.reset(feedback_);
    mixSm_.reset(mix_);
    inputGainSm_.reset(inputGain_);
    outputGainSm_.reset(outputGain_);
    toneSm_.reset(toneHz_);
    smearSm_.reset(smear_);
    appliedToneHz_ = toneHz_;
    appliedSmear_ = smear_;
    heavyParamCadenceCountdown_ = 1u;
    heavyParamCadenceHit_ = false;
    smoothTargetsDirty_ = false;
    appliedParamVersion_ = pendingParamVersion_.load(std::memory_order_acquire);
    delayL_.clear();
    delayR_.clear();
    lowpassL_.reset();
    lowpassR_.reset();
    dcL_.reset();
    dcR_.reset();
    diffuserL_.reset();
    diffuserR_.reset();
}

bool OrbitDelayCore::attachBuffers(float* leftBuffer, float* rightBuffer, uint32_t size) {
    if (rightBuffer == nullptr) {
        initialized_ = false;
        return false;
    }
    return attachBuffersImpl(delayL_, delayR_, initialized_, leftBuffer, size, rightBuffer, size, kMinUsefulDelaySize);
}

bool OrbitDelayCore::attachBufferMono(float* buffer, uint32_t size) {
    return attachBuffersImpl(delayL_, delayR_, initialized_, buffer, size, nullptr, 0u, kMinUsefulDelaySize);
}

bool OrbitDelayCore::attachBuffers(float* leftBuffer, uint32_t leftSize, float* rightBuffer, uint32_t rightSize) {
    return attachBuffersImpl(delayL_, delayR_, initialized_, leftBuffer, leftSize, rightBuffer, rightSize, kMinUsefulDelaySize);
}

void OrbitDelayCore::setSampleRate(float sr) {
    const float sanitized = sanitizeFinite(sr, kFallbackSampleRate);
    const float clamped = (sanitized <= 1.0f) ? kFallbackSampleRate : clampf(sanitized, 1.0f, 384000.0f);
    pendingSampleRate_.store(clamped, std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setOrbit(float value) {
    pendingOrbit_.store(clampf(sanitizeFinite(value, orbit_), 0.25f, 3.0f), std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setOffsetSamples(float value) {
    pendingOffsetSamples_.store(clampf(sanitizeFinite(value, offsetSamples_), -200000.0f, 200000.0f), std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setTempoBpm(float value) {
    pendingTempoBpm_.store(clampf(sanitizeFinite(value, tempoBpm_), kMinTempoBpm, kMaxTempoBpm), std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setNoteDivision(float value) {
    pendingNoteDivision_.store(clampf(sanitizeFinite(value, noteDivision_), kMinNoteDivision, kMaxNoteDivision), std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setStereoSpread(float value) {
    pendingStereoSpread_.store(clampf(sanitizeFinite(value, stereoSpread_), 0.0f, kStereoSpreadMax), std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setFeedback(float value) {
    pendingFeedback_.store(clampf(sanitizeFinite(value, feedback_), 0.0f, 0.95f), std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setMix(float value) {
    pendingMix_.store(clampf(sanitizeFinite(value, mix_), 0.0f, 1.0f), std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setInputGain(float value) {
    pendingInputGain_.store(clampf(sanitizeFinite(value, inputGain_), 0.0f, 4.0f), std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setOutputGain(float value) {
    pendingOutputGain_.store(clampf(sanitizeFinite(value, outputGain_), 0.0f, 4.0f), std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setToneHz(float value) {
    pendingToneHz_.store(clampf(sanitizeFinite(value, toneHz_), 300.0f, 12000.0f), std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setSmearAmount(float value) {
    pendingSmear_.store(clampf(sanitizeFinite(value, smear_), 0.0f, 1.0f), std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setShimmerMode(bool enabled) {
    pendingShimmerModeEnabled_.store(enabled, std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setLowpassCutoffHz(float value) {
    setToneHz(value);
}

void OrbitDelayCore::setDiffusion(float value) {
    setSmearAmount(value);
}

void OrbitDelayCore::setDiffuserStages(uint32_t count) {
    const uint32_t clamped = (count > AllpassDiffuser::kMaxStages) ? AllpassDiffuser::kMaxStages : count;
    pendingDiffuserStages_.store(clamped, std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setDcBlockEnabled(bool enabled) {
    pendingDcBlockEnabled_.store(enabled, std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

void OrbitDelayCore::setReadMode(ReadMode mode) {
    pendingReadMode_.store(static_cast<uint32_t>(mode), std::memory_order_relaxed);
    pendingParamVersion_.fetch_add(1u, std::memory_order_release);
}

float OrbitDelayCore::processChannelFast(float input, DelayLine& delay, BiquadLowpass& lp, DCBlocker& dc, AllpassDiffuser& diffuser,
                                         const SmoothedParams& params, float spread, float delaySize, float invDelaySize) {
    const float sanitizedInput = input;
    float wet = 0.0f;
    if (readMode_ == ReadMode::AccidentalReverse) {
        const float delaySamples = params.tempoDelaySamples + spread;
        float readPosForward = static_cast<float>(delay.writePos) + delaySamples;
        while (readPosForward >= delaySize) {
            readPosForward -= delaySize;
        }
        while (readPosForward < 0.0f) {
            readPosForward += delaySize;
        }

        const int32_t delayBack = static_cast<int32_t>(delaySize) - static_cast<int32_t>(readPosForward);
        const int32_t writePosInt = static_cast<int32_t>(delay.writePos);
        const float readPos = wrapPosFloat(static_cast<float>(writePosInt - delayBack), delaySize, invDelaySize);
        wet = delay.readAbsoluteLinearWrapped(readPos);
    } else {
        float readPos = params.orbit * static_cast<float>(delay.writePos) + params.offsetSamples + spread;
        readPos = wrapPosFloat(readPos, delaySize, invDelaySize);

#if defined(ORBIT_DELAY_ENABLE_HERMITE)
        wet = delay.readAbsoluteHermiteWrapped(readPos);
#else
        wet = delay.readAbsoluteLinearWrapped(readPos);
#endif
    }

    if (!isFiniteSafe(wet)) {
        wet = 0.0f;
    }

    if (shimmerModeEnabled_) {
        wet = diffuser.process(wet);
        if (!isFiniteSafe(wet)) {
            diffuser.reset();
            wet = 0.0f;
        }
    }

    float filteredWet = lp.process(wet);
    if (!isFiniteSafe(filteredWet)) {
        lp.reset();
        filteredWet = 0.0f;
    }

    const float fb = filteredWet * params.feedback;

    float toBuffer = sanitizedInput * params.inputGain + fb;
    if (!isFiniteSafe(toBuffer)) {
        toBuffer = 0.0f;
    }

    if (dcBlockEnabled_) {
        toBuffer = dc.process(toBuffer);
        if (!isFiniteSafe(toBuffer)) {
            dc.reset();
            toBuffer = 0.0f;
        }
    }

    delay.write(toBuffer);

    const float out = (sanitizedInput * (1.0f - params.mix) + wet * params.mix) * params.outputGain;
    return isFiniteSafe(out) ? out : 0.0f;
}

float OrbitDelayCore::processChannel(float input, DelayLine& delay, BiquadLowpass& lp, DCBlocker& dc, AllpassDiffuser& diffuser,
                                     const SmoothedParams& params, float spread) {
    const float sanitizedInput = sanitizeFinite(input, 0.0f);
    if (!initialized_ || delay.buffer == nullptr || delay.size < kMinUsefulDelaySize) {
        return dryPassThrough(sanitizedInput);
    }

    const float delaySize = static_cast<float>(delay.size);
    const float invDelaySize = 1.0f / delaySize;
    return processChannelFast(sanitizedInput, delay, lp, dc, diffuser, params, spread, delaySize, invDelaySize);
}

float OrbitDelayCore::processSampleMono(float input) {
    syncDspParams();
    updateSmoothedTargetsIfDirty();
    const SmoothedParams params = advanceSmoothers();
    return processChannel(input, delayL_, lowpassL_, dcL_, diffuserL_, params, 0.0f);
}

void OrbitDelayCore::processSampleStereo(float inL, float inR, float& outL, float& outR) {
    syncDspParams();
    updateSmoothedTargetsIfDirty();
    const SmoothedParams params = advanceSmoothers();
    outL = processChannel(inL, delayL_, lowpassL_, dcL_, diffuserL_, params, -params.stereoSpread);
    outR = processChannel(inR, delayR_, lowpassR_, dcR_, diffuserR_, params, params.stereoSpread);
}

void OrbitDelayCore::processMono(const float* input, float* output, uint32_t numSamples) {
    if (input == nullptr || output == nullptr) {
        return;
    }

    syncDspParams();
    updateSmoothedTargetsIfDirty();
    const bool canProcess = initialized_ && delayL_.buffer != nullptr && delayL_.size >= kMinUsefulDelaySize;
    if (!canProcess) {
        for (uint32_t i = 0; i < numSamples; ++i) {
            const SmoothedParams params = advanceSmoothers();
            (void)params;
            const float sanitizedInput = sanitizeFinite(input[i], 0.0f);
            output[i] = dryPassThrough(sanitizedInput);
        }
        return;
    }

    const float delaySize = static_cast<float>(delayL_.size);
    const float invDelaySize = 1.0f / delaySize;
    for (uint32_t i = 0; i < numSamples; ++i) {
        const SmoothedParams params = advanceSmoothers();
        const float sanitizedInput = sanitizeFinite(input[i], 0.0f);
        output[i] = processChannelFast(sanitizedInput, delayL_, lowpassL_, dcL_, diffuserL_, params, 0.0f, delaySize, invDelaySize);
    }
}

void OrbitDelayCore::processStereo(const float* inputL, const float* inputR, float* outputL, float* outputR, uint32_t numSamples) {
    if (inputL == nullptr || inputR == nullptr || outputL == nullptr || outputR == nullptr) {
        return;
    }

    syncDspParams();
    updateSmoothedTargetsIfDirty();
    const bool canProcessL = initialized_ && delayL_.buffer != nullptr && delayL_.size >= kMinUsefulDelaySize;
    const bool canProcessR = initialized_ && delayR_.buffer != nullptr && delayR_.size >= kMinUsefulDelaySize;
    const float delaySizeL = static_cast<float>(delayL_.size);
    const float delaySizeR = static_cast<float>(delayR_.size);
    const float invDelaySizeL = canProcessL ? (1.0f / delaySizeL) : 0.0f;
    const float invDelaySizeR = canProcessR ? (1.0f / delaySizeR) : 0.0f;

    if (canProcessL && canProcessR) {
        for (uint32_t i = 0; i < numSamples; ++i) {
            const SmoothedParams params = advanceSmoothers();
            const float inLSafe = sanitizeFinite(inputL[i], 0.0f);
            const float inRSafe = sanitizeFinite(inputR[i], 0.0f);
            outputL[i] =
                processChannelFast(inLSafe, delayL_, lowpassL_, dcL_, diffuserL_, params, -params.stereoSpread, delaySizeL, invDelaySizeL);
            outputR[i] =
                processChannelFast(inRSafe, delayR_, lowpassR_, dcR_, diffuserR_, params, params.stereoSpread, delaySizeR, invDelaySizeR);
        }
        return;
    }

    if (canProcessL) {
        for (uint32_t i = 0; i < numSamples; ++i) {
            const SmoothedParams params = advanceSmoothers();
            const float inLSafe = sanitizeFinite(inputL[i], 0.0f);
            const float inRSafe = sanitizeFinite(inputR[i], 0.0f);
            outputL[i] =
                processChannelFast(inLSafe, delayL_, lowpassL_, dcL_, diffuserL_, params, -params.stereoSpread, delaySizeL, invDelaySizeL);
            outputR[i] = dryPassThrough(inRSafe);
        }
        return;
    }

    if (canProcessR) {
        for (uint32_t i = 0; i < numSamples; ++i) {
            const SmoothedParams params = advanceSmoothers();
            const float inLSafe = sanitizeFinite(inputL[i], 0.0f);
            const float inRSafe = sanitizeFinite(inputR[i], 0.0f);
            outputL[i] = dryPassThrough(inLSafe);
            outputR[i] =
                processChannelFast(inRSafe, delayR_, lowpassR_, dcR_, diffuserR_, params, params.stereoSpread, delaySizeR, invDelaySizeR);
        }
        return;
    }

    for (uint32_t i = 0; i < numSamples; ++i) {
        const SmoothedParams params = advanceSmoothers();
        (void)params;
        const float inLSafe = sanitizeFinite(inputL[i], 0.0f);
        const float inRSafe = sanitizeFinite(inputR[i], 0.0f);
        outputL[i] = dryPassThrough(inLSafe);
        outputR[i] = dryPassThrough(inRSafe);
    }
}

} // namespace orbit::dsp
