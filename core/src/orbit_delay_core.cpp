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

void OrbitDelayCore::syncDspParams() {
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
        tempoDelaySamples_ = computeTempoDelaySamples(sampleRate_, tempoBpm_, noteDivision_);
        smoothTargetsDirty_ = true;
        sampleRateDirty_ = false;
        lowpassDirty_ = true;
    }

    if (lowpassDirty_) {
        const float nyquistLimited = clampf(toneHz_, 300.0f, clampf(0.49f * sampleRate_, 300.0f, 12000.0f));
        lowpassL_.setCutoffHz(nyquistLimited);
        lowpassR_.setCutoffHz(nyquistLimited);
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
        lowpassL_.setCutoffHz(clampedToneHz);
        lowpassR_.setCutoffHz(clampedToneHz);
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
    toneSm_.reset(toneHz_);
    smearSm_.reset(smear_);
    appliedToneHz_ = toneHz_;
    appliedSmear_ = smear_;
    heavyParamCadenceCountdown_ = 1u;
    heavyParamCadenceHit_ = false;
    smoothTargetsDirty_ = false;
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
    if (sampleRate_ != clamped) {
        sampleRate_ = clamped;
        sampleRateDirty_ = true;
    }
}

void OrbitDelayCore::setOrbit(float value) {
    orbit_ = clampf(sanitizeFinite(value, orbit_), 0.25f, 3.0f);
    smoothTargetsDirty_ = true;
}

void OrbitDelayCore::setOffsetSamples(float value) {
    offsetSamples_ = clampf(sanitizeFinite(value, offsetSamples_), -200000.0f, 200000.0f);
    smoothTargetsDirty_ = true;
}

void OrbitDelayCore::setTempoBpm(float value) {
    tempoBpm_ = clampf(sanitizeFinite(value, tempoBpm_), kMinTempoBpm, kMaxTempoBpm);
    tempoDelaySamples_ = computeTempoDelaySamples(sampleRate_, tempoBpm_, noteDivision_);
    smoothTargetsDirty_ = true;
}

void OrbitDelayCore::setNoteDivision(float value) {
    noteDivision_ = clampf(sanitizeFinite(value, noteDivision_), kMinNoteDivision, kMaxNoteDivision);
    tempoDelaySamples_ = computeTempoDelaySamples(sampleRate_, tempoBpm_, noteDivision_);
    smoothTargetsDirty_ = true;
}

void OrbitDelayCore::setStereoSpread(float value) {
    stereoSpread_ = clampf(sanitizeFinite(value, stereoSpread_), 0.0f, kStereoSpreadMax);
    smoothTargetsDirty_ = true;
}

void OrbitDelayCore::setFeedback(float value) {
    feedback_ = clampf(sanitizeFinite(value, feedback_), 0.0f, 0.95f);
    smoothTargetsDirty_ = true;
}

void OrbitDelayCore::setMix(float value) {
    mix_ = clampf(sanitizeFinite(value, mix_), 0.0f, 1.0f);
    smoothTargetsDirty_ = true;
}

void OrbitDelayCore::setInputGain(float value) {
    inputGain_ = clampf(sanitizeFinite(value, inputGain_), 0.0f, 4.0f);
}

void OrbitDelayCore::setOutputGain(float value) {
    outputGain_ = clampf(sanitizeFinite(value, outputGain_), 0.0f, 4.0f);
}

void OrbitDelayCore::setToneHz(float value) {
    toneHz_ = clampf(sanitizeFinite(value, toneHz_), 300.0f, 12000.0f);
    smoothTargetsDirty_ = true;
}

void OrbitDelayCore::setSmearAmount(float value) {
    smear_ = clampf(sanitizeFinite(value, smear_), 0.0f, 1.0f);
    smoothTargetsDirty_ = true;
}

void OrbitDelayCore::setLowpassCutoffHz(float value) {
    setToneHz(value);
}

void OrbitDelayCore::setDiffusion(float value) {
    setSmearAmount(value);
}

void OrbitDelayCore::setDiffuserStages(uint32_t count) {
    diffuserStages_ = (count > AllpassDiffuser::kMaxStages) ? AllpassDiffuser::kMaxStages : count;
    diffuserDirty_ = true;
}

void OrbitDelayCore::setDcBlockEnabled(bool enabled) {
    dcBlockEnabled_ = enabled;
}

void OrbitDelayCore::setReadMode(ReadMode mode) {
    readMode_ = mode;
}

float OrbitDelayCore::processChannelFast(float input, DelayLine& delay, OnePoleLowpass& lp, DCBlocker& dc, AllpassDiffuser& diffuser,
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

    wet = diffuser.process(wet);
    if (!isFiniteSafe(wet)) {
        diffuser.reset();
        wet = 0.0f;
    }

    float filteredWet = lp.process(wet);
    if (!isFiniteSafe(filteredWet)) {
        lp.reset(0.0f);
        filteredWet = 0.0f;
    }

    const float fb = filteredWet * params.feedback;

    float toBuffer = sanitizedInput * inputGain_ + fb;
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

    const float out = (sanitizedInput * (1.0f - params.mix) + wet * params.mix) * outputGain_;
    return isFiniteSafe(out) ? out : 0.0f;
}

float OrbitDelayCore::processChannel(float input, DelayLine& delay, OnePoleLowpass& lp, DCBlocker& dc, AllpassDiffuser& diffuser,
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
