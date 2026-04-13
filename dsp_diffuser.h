#pragma once

#include <cstdint>

#include "dsp_utils.h"

namespace orbit::dsp {

struct AllpassStage {
    static constexpr uint32_t kMaxDelay = 128;

    float buffer[kMaxDelay] = {};
    uint32_t delaySamples = 32;
    uint32_t writePos = 0;
    float gain = 0.5f;

    void reset() {
        for (uint32_t i = 0; i < kMaxDelay; ++i) {
            buffer[i] = 0.0f;
        }
        writePos = 0;
    }

    void setDelaySamples(uint32_t d) {
        delaySamples = (d < 1u) ? 1u : ((d >= kMaxDelay) ? (kMaxDelay - 1u) : d);
        writePos = 0;
    }

    void setGain(float g) {
        gain = clampf(g, -0.95f, 0.95f);
    }

    float process(float x) {
        const int32_t readPos = wrapIndexInt(static_cast<int32_t>(writePos) - static_cast<int32_t>(delaySamples), delaySamples);
        const float d = buffer[readPos];
        const float y = d - gain * x;
        buffer[writePos] = x + gain * y;
        writePos = wrapIncrement(writePos, delaySamples);
        return y;
    }
};

struct AllpassDiffuser {
    static constexpr uint32_t kMaxStages = 4;

    AllpassStage stages[kMaxStages];
    uint32_t stageCount = 2;
    float amount = 0.0f;

    void reset() {
        static constexpr uint32_t kDelays[kMaxStages] = {23, 41, 67, 97};
        for (uint32_t i = 0; i < kMaxStages; ++i) {
            stages[i].setDelaySamples(kDelays[i]);
            stages[i].reset();
        }
    }

    void setStageCount(uint32_t count) {
        stageCount = (count > kMaxStages) ? kMaxStages : count;
    }

    void setAmount(float amt) {
        amount = clampf(amt, 0.0f, 1.0f);
        const float g = 0.2f + amount * 0.65f;
        for (uint32_t i = 0; i < stageCount; ++i) {
            stages[i].setGain(g);
        }
    }

    float process(float x) {
        if (amount <= 0.0f || stageCount == 0u) {
            return x;
        }
        float y = x;
        for (uint32_t i = 0; i < stageCount; ++i) {
            y = stages[i].process(y);
        }
        return y;
    }
};

} // namespace orbit::dsp
