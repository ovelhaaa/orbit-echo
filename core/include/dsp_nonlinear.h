#pragma once

#include <cmath>

#include "core/include/dsp_utils.h"

namespace orbit::dsp {

inline float softClipPolynomial(float x) {
    if (!isFiniteSafe(x)) {
        return 0.0f;
    }

    const float clamped = clampf(x, -1.5f, 1.5f);
    const float x2 = clamped * clamped;
    return clamped * (1.0f - (x2 * (1.0f / 6.75f)));
}

inline float lowCostLimiterGain(float envelope, float threshold) {
    const float safeThreshold = clampf(threshold, 0.001f, 4.0f);
    if (!isFiniteSafe(envelope) || envelope <= safeThreshold) {
        return 1.0f;
    }
    return safeThreshold / envelope;
}

struct EnvelopeFollowerLimiter {
    float envelope = 0.0f;
    float attackCoeff = 0.01f;
    float releaseCoeff = 0.001f;

    void configure(float sampleRate, float attackMs = 2.0f, float releaseMs = 75.0f) {
        const float sr = clampf(sampleRate, 1.0f, 384000.0f);
        attackCoeff = timeToCoeff(sr, attackMs);
        releaseCoeff = timeToCoeff(sr, releaseMs);
    }

    void reset() {
        envelope = 0.0f;
    }

    float process(float input, float threshold) {
        const float level = isFiniteSafe(input) ? std::fabs(input) : 0.0f;
        const float coeff = (level > envelope) ? attackCoeff : releaseCoeff;
        envelope += (level - envelope) * coeff;
        if (!isFiniteSafe(envelope)) {
            envelope = 0.0f;
        }
        return input * lowCostLimiterGain(envelope, threshold);
    }

private:
    static float timeToCoeff(float sampleRate, float timeMs) {
        const float safeMs = clampf(timeMs, 0.01f, 5000.0f);
        return 1.0f - std::exp(-1.0f / (0.001f * safeMs * sampleRate));
    }
};

inline float processDrivenSoftClipLimiter(float input,
                                          EnvelopeFollowerLimiter& limiter,
                                          float drive,
                                          float amount,
                                          float threshold) {
    if (!isFiniteSafe(input)) {
        return 0.0f;
    }

    const float mix = clampf(amount, 0.0f, 1.0f);
    if (mix <= 0.0f) {
        return input;
    }

    const float safeDrive = clampf(drive, 1.0f, 16.0f);
    const float compensated = softClipPolynomial(input * safeDrive) / safeDrive;
    const float limited = limiter.process(compensated, threshold);
    const float processed = isFiniteSafe(limited) ? limited : 0.0f;
    return input + (processed - input) * mix;
}

} // namespace orbit::dsp
