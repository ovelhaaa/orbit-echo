#include "orbit_delay_core.h"

#include "dsp_utils.h"

namespace orbit::dsp {

float OrbitDelayCore::sanitizeFinite(float value, float fallback) {
    return isFiniteSafe(value) ? value : fallback;
}

float OrbitDelayCore::dryPassThrough(float input) const {
    return sanitizeFinite(input * outputGain_, 0.0f);
}

void OrbitDelayCore::syncDspParams() {
    if (sampleRateDirty_) {
        lowpassL_.setSampleRate(sampleRate_);
        lowpassR_.setSampleRate(sampleRate_);
        dcL_.setSampleRate(sampleRate_);
        dcR_.setSampleRate(sampleRate_);
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
        diffuserL_.setAmount(smear_);
        diffuserR_.setAmount(smear_);
        diffuserDirty_ = false;
    }
}

void OrbitDelayCore::reset(float sampleRate) {
    setSampleRate(sampleRate);
    syncDspParams();
    delayL_.clear();
    delayR_.clear();
    lowpassL_.reset();
    lowpassR_.reset();
    dcL_.reset();
    dcR_.reset();
    diffuserL_.reset();
    diffuserR_.reset();
}

bool OrbitDelayCore::attachBuffers(float* leftBuffer, uint32_t leftSize, float* rightBuffer, uint32_t rightSize) {
    if (leftBuffer == nullptr || leftSize < kMinUsefulDelaySize) {
        initialized_ = false;
        return false;
    }

    if (rightBuffer != nullptr && rightSize < kMinUsefulDelaySize) {
        initialized_ = false;
        return false;
    }

    const bool leftOk = delayL_.attach(leftBuffer, leftSize);
    const bool rightRequested = (rightBuffer != nullptr);
    const bool rightOk = rightRequested ? delayR_.attach(rightBuffer, rightSize) : false;

    if (!rightRequested) {
        delayR_.buffer = nullptr;
        delayR_.size = 0;
        delayR_.writePos = 0;
    }

    initialized_ = leftOk && (!rightRequested || rightOk);
    return initialized_;
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
}

void OrbitDelayCore::setOffsetSamples(float value) {
    offsetSamples_ = clampf(sanitizeFinite(value, offsetSamples_), -200000.0f, 200000.0f);
}

void OrbitDelayCore::setStereoSpread(float value) {
    stereoSpread_ = clampf(sanitizeFinite(value, stereoSpread_), 0.0f, kStereoSpreadMax);
}

void OrbitDelayCore::setFeedback(float value) {
    feedback_ = clampf(sanitizeFinite(value, feedback_), 0.0f, 0.95f);
}

void OrbitDelayCore::setMix(float value) {
    mix_ = clampf(sanitizeFinite(value, mix_), 0.0f, 1.0f);
}

void OrbitDelayCore::setInputGain(float value) {
    inputGain_ = clampf(sanitizeFinite(value, inputGain_), 0.0f, 4.0f);
}

void OrbitDelayCore::setOutputGain(float value) {
    outputGain_ = clampf(sanitizeFinite(value, outputGain_), 0.0f, 4.0f);
}

void OrbitDelayCore::setLowpassCutoffHz(float value) {
    toneHz_ = clampf(sanitizeFinite(value, toneHz_), 300.0f, 12000.0f);
    lowpassDirty_ = true;
}

void OrbitDelayCore::setDiffusion(float value) {
    smear_ = clampf(sanitizeFinite(value, smear_), 0.0f, 1.0f);
    diffuserDirty_ = true;
}

void OrbitDelayCore::setDiffuserStages(uint32_t count) {
    diffuserStages_ = (count > AllpassDiffuser::kMaxStages) ? AllpassDiffuser::kMaxStages : count;
    diffuserDirty_ = true;
}

void OrbitDelayCore::setDcBlockEnabled(bool enabled) {
    dcBlockEnabled_ = enabled;
}

float OrbitDelayCore::processChannel(float input, DelayLine& delay, OnePoleLowpass& lp, DCBlocker& dc, AllpassDiffuser& diffuser, float spread) {
    const float sanitizedInput = sanitizeFinite(input, 0.0f);
    if (!initialized_ || delay.buffer == nullptr || delay.size < kMinUsefulDelaySize) {
        return dryPassThrough(sanitizedInput);
    }

    syncDspParams();

    float readPos = orbit_ * static_cast<float>(delay.writePos) + offsetSamples_ + spread;
    readPos = wrapPosFloat(readPos, static_cast<float>(delay.size));

#if defined(ORBIT_DELAY_ENABLE_HERMITE)
    float wet = delay.readAbsoluteHermite(readPos);
#else
    float wet = delay.readAbsoluteLinear(readPos);
#endif

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

    const float fb = filteredWet * feedback_;

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

    const float out = (sanitizedInput * (1.0f - mix_) + wet * mix_) * outputGain_;
    return isFiniteSafe(out) ? out : 0.0f;
}

float OrbitDelayCore::processSampleMono(float input) {
    return processChannel(input, delayL_, lowpassL_, dcL_, diffuserL_, 0.0f);
}

void OrbitDelayCore::processSampleStereo(float inL, float inR, float& outL, float& outR) {
    outL = processChannel(inL, delayL_, lowpassL_, dcL_, diffuserL_, -stereoSpread_);
    outR = processChannel(inR, delayR_, lowpassR_, dcR_, diffuserR_, stereoSpread_);
}

void OrbitDelayCore::processMono(const float* input, float* output, uint32_t numSamples) {
    if (input == nullptr || output == nullptr) {
        return;
    }

    for (uint32_t i = 0; i < numSamples; ++i) {
        output[i] = processSampleMono(input[i]);
    }
}

void OrbitDelayCore::processStereo(const float* inputL, const float* inputR, float* outputL, float* outputR, uint32_t numSamples) {
    if (inputL == nullptr || inputR == nullptr || outputL == nullptr || outputR == nullptr) {
        return;
    }

    for (uint32_t i = 0; i < numSamples; ++i) {
        processSampleStereo(inputL[i], inputR[i], outputL[i], outputR[i]);
    }
}

} // namespace orbit::dsp
