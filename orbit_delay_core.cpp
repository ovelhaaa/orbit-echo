#include "orbit_delay_core.h"

#include "dsp_utils.h"

namespace orbit::dsp {

void OrbitDelayCore::reset(float sampleRate) {
    setSampleRate(sampleRate);
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
    const bool leftOk = delayL_.attach(leftBuffer, leftSize);
    const bool rightRequested = (rightBuffer != nullptr && rightSize > 1u);
    const bool rightOk = rightRequested ? delayR_.attach(rightBuffer, rightSize) : false;

    if (!rightRequested) {
        delayR_.buffer = nullptr;
        delayR_.size = 0;
        delayR_.writePos = 0;
    }

    return leftOk && (!rightRequested || rightOk);
}

void OrbitDelayCore::setSampleRate(float sr) {
    sampleRate_ = clampf(sr, 1.0f, 384000.0f);
    lowpassL_.setSampleRate(sampleRate_);
    lowpassR_.setSampleRate(sampleRate_);
    dcL_.setSampleRate(sampleRate_);
    dcR_.setSampleRate(sampleRate_);
}

void OrbitDelayCore::setOrbit(float value) {
    orbit_ = clampf(value, 0.0f, 1.0f);
}

void OrbitDelayCore::setOffsetSamples(float value) {
    offsetSamples_ = clampf(value, -200000.0f, 200000.0f);
}

void OrbitDelayCore::setStereoSpread(float value) {
    stereoSpread_ = clampf(value, -20000.0f, 20000.0f);
}

void OrbitDelayCore::setFeedback(float value) {
    feedback_ = clampf(value, 0.0f, 0.995f);
}

void OrbitDelayCore::setMix(float value) {
    mix_ = clampf(value, 0.0f, 1.0f);
}

void OrbitDelayCore::setInputGain(float value) {
    inputGain_ = clampf(value, 0.0f, 4.0f);
}

void OrbitDelayCore::setOutputGain(float value) {
    outputGain_ = clampf(value, 0.0f, 4.0f);
}

void OrbitDelayCore::setLowpassCutoffHz(float value) {
    lowpassL_.setCutoffHz(value);
    lowpassR_.setCutoffHz(value);
}

void OrbitDelayCore::setDiffusion(float value) {
    diffuserL_.setAmount(value);
    diffuserR_.setAmount(value);
}

void OrbitDelayCore::setDiffuserStages(uint32_t count) {
    diffuserL_.setStageCount(count);
    diffuserR_.setStageCount(count);
}

void OrbitDelayCore::setDcBlockEnabled(bool enabled) {
    dcBlockEnabled_ = enabled;
}

float OrbitDelayCore::processChannel(float input, DelayLine& delay, OnePoleLowpass& lp, DCBlocker& dc, AllpassDiffuser& diffuser, float spread) {
    if (delay.buffer == nullptr || delay.size < 2u) {
        return input * outputGain_;
    }

    float readPos = orbit_ * static_cast<float>(delay.writePos) + offsetSamples_ + spread;
    readPos = wrapPosFloat(readPos, static_cast<float>(delay.size));

#if defined(ORBIT_DELAY_ENABLE_HERMITE)
    float wet = delay.readAbsoluteHermite(readPos);
#else
    float wet = delay.readAbsoluteLinear(readPos);
#endif

    wet = diffuser.process(wet);
    const float fb = lp.process(wet) * feedback_;

    float toBuffer = input * inputGain_ + fb;
    if (dcBlockEnabled_) {
        toBuffer = dc.process(toBuffer);
    }

    if (!isFiniteSafe(toBuffer)) {
        toBuffer = 0.0f;
    }

    delay.write(toBuffer);

    const float out = (input * (1.0f - mix_) + wet * mix_) * outputGain_;
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
