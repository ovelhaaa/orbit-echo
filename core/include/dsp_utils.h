#pragma once

#include <cstdint>
#include <cmath>

namespace orbit::dsp {

static constexpr float kPi = 3.14159265358979323846f;

inline float clampf(float x, float lo, float hi) {
    return (x < lo) ? lo : ((x > hi) ? hi : x);
}

inline bool isFiniteSafe(float x) {
    return std::isfinite(x);
}

inline int32_t wrapIndexInt(int32_t idx, uint32_t size) {
    const int32_t s = static_cast<int32_t>(size);
    if (s <= 0) {
        return 0;
    }
    idx %= s;
    if (idx < 0) {
        idx += s;
    }
    return idx;
}

inline float wrapPosFloat(float pos, float size) {
    if (size <= 0.0f) {
        return 0.0f;
    }
    const float invSize = 1.0f / size;
    const float wrapped = pos - size * std::floor(pos * invSize);
    return (wrapped >= size) ? (wrapped - size) : wrapped;
}

inline float wrapPosFloat(float pos, float size, float invSize) {
    if (size <= 0.0f || invSize <= 0.0f) {
        return 0.0f;
    }
    const float wrapped = pos - size * std::floor(pos * invSize);
    return (wrapped >= size) ? (wrapped - size) : wrapped;
}

inline uint32_t wrapIncrement(uint32_t idx, uint32_t size) {
    ++idx;
    return (idx >= size) ? 0u : idx;
}

} // namespace orbit::dsp
