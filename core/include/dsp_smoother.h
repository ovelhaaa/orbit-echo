#pragma once

#include <cstdint>

#include "core/include/dsp_utils.h"

namespace orbit::dsp {

struct LinearSmoother {
    float current = 0.0f;
    float target = 0.0f;
    float step = 0.0f;
    float sampleRate = 48000.0f;
    float timeMs = 20.0f;
    uint32_t samplesRemaining = 0u;

    void configure(float sr, float smoothingMs) {
        sampleRate = clampf(sr, 1.0f, 384000.0f);
        timeMs = clampf(smoothingMs, 0.0f, 5000.0f);
        recalcStep();
    }

    void setTarget(float value) {
        target = value;
        recalcStep();
    }

    void reset(float value) {
        current = value;
        target = value;
        step = 0.0f;
        samplesRemaining = 0u;
    }

    float next() {
        if (samplesRemaining > 0u) {
            --samplesRemaining;
            current += step;
            if (samplesRemaining == 0u) {
                current = target;
            }
        } else {
            current = target;
        }
        return current;
    }

private:
    void recalcStep() {
        if (timeMs <= 0.0f || sampleRate <= 0.0f) {
            samplesRemaining = 0u;
            step = 0.0f;
            current = target;
            return;
        }

        const float totalSamplesF = (timeMs * 0.001f) * sampleRate;
        const uint32_t totalSamples = static_cast<uint32_t>(totalSamplesF);
        samplesRemaining = (totalSamples < 1u) ? 1u : totalSamples;
        step = (target - current) / static_cast<float>(samplesRemaining);
    }
};

} // namespace orbit::dsp
