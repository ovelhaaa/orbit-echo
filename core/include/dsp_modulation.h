#pragma once

#include <cstdint>

#include "core/include/dsp_utils.h"

namespace orbit::dsp {

/**
 * Lightweight bipolar LFO for delay-time modulation.
 *
 * The oscillator keeps phase in normalized cycles and renders a parabolic
 * approximation derived from a triangle wave, avoiding per-sample trigonometry
 * and lookup-table memory. Configuration is intentionally split from advance()
 * so callers can smooth rate/depth outside the channel processing hot path.
 */
struct ParabolicLfo {
    float phase = 0.0f;
    float phaseIncrement = 0.0f;
    float depthSamples = 0.0f;

    void reset(float phaseCycles = 0.0f) {
        phase = wrapUnit(phaseCycles);
    }

    void setRateHz(float rateHz, float sampleRate) {
        const float safeRate = clampf(isFiniteSafe(rateHz) ? rateHz : 0.0f, 0.0f, 20.0f);
        const float safeSampleRate = clampf(isFiniteSafe(sampleRate) ? sampleRate : 48000.0f, 1.0f, 384000.0f);
        phaseIncrement = safeRate / safeSampleRate;
    }

    void setDepthSamples(float samples) {
        depthSamples = clampf(isFiniteSafe(samples) ? samples : 0.0f, 0.0f, 20000.0f);
    }

    float nextSamples() {
        const float value = depthSamples * parabolicBipolar(phase);
        phase += phaseIncrement;
        if (phase >= 1.0f) {
            phase -= static_cast<float>(static_cast<uint32_t>(phase));
            if (phase >= 1.0f) {
                phase -= 1.0f;
            }
        }
        return value;
    }

    static float parabolicBipolar(float phaseCycles) {
        const float p = wrapUnit(phaseCycles);
        const float triangle = (p < 0.5f) ? ((4.0f * p) - 1.0f) : (3.0f - (4.0f * p));
        return triangle * (2.0f - absf(triangle));
    }

private:
    static float absf(float x) {
        return (x < 0.0f) ? -x : x;
    }

    static float wrapUnit(float x) {
        if (!isFiniteSafe(x)) {
            return 0.0f;
        }
        while (x >= 1.0f) {
            x -= 1.0f;
        }
        while (x < 0.0f) {
            x += 1.0f;
        }
        return x;
    }
};

} // namespace orbit::dsp
