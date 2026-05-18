#include <cmath>
#include <iostream>

#include "core/include/dsp_filters.h"
#include "core/include/dsp_nonlinear.h"

namespace {

bool nearlyEqual(float a, float b, float eps = 1.0e-6f) {
    return std::fabs(a - b) <= eps;
}

int fail(const char* message) {
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

} // namespace

int main() {
    using orbit::dsp::DCBlocker;
    using orbit::dsp::BiquadLowpass;
    using orbit::dsp::EnvelopeFollowerLimiter;
    using orbit::dsp::processDrivenSoftClipLimiter;
    using orbit::dsp::softClipPolynomial;

    // BiquadLowpass: limites/clamps e estabilidade básica.
    BiquadLowpass lp;
    lp.setSampleRate(-100.0f);   // clamp para 1 Hz mínimo
    lp.setCutoffHz(99999.0f);    // clamp para 0.49 * sampleRate
    if (!(lp.sampleRate >= 1.0f && lp.sampleRate <= 384000.0f)) {
        return fail("BiquadLowpass sampleRate clamp out of range");
    }
    const float lpMaxCutoff = (0.49f * lp.sampleRate < 1.0f) ? 1.0f : (0.49f * lp.sampleRate);
    if (!(lp.cutoffHz >= 0.0f && lp.cutoffHz <= lpMaxCutoff + 1.0e-6f)) {
        return fail("BiquadLowpass cutoff clamp out of range");
    }
    if (!std::isfinite(lp.b0) || !std::isfinite(lp.b1) || !std::isfinite(lp.b2) || !std::isfinite(lp.a1) || !std::isfinite(lp.a2)) {
        return fail("BiquadLowpass coefficients should remain finite");
    }

    lp.reset(0.0f);
    float y2 = 0.0f;
    for (int i = 0; i < 128; ++i) {
        y2 = lp.process(1.0f);
    }
    if (!std::isfinite(y2) || y2 < 0.0f || y2 > 1.2f) {
        return fail("BiquadLowpass step response should stay finite and bounded");
    }

    lp.setQ(0.5f);
    const float yQ = lp.process(0.5f);
    if (!std::isfinite(yQ) || lp.q < 0.1f || lp.q > 10.0f) {
        return fail("BiquadLowpass Q update should be finite and clamped");
    }

    // DCBlocker: limites/clamps e remoção de componente DC.
    DCBlocker dc;
    dc.setSampleRate(500000.0f); // clamp para 384000
    dc.setCutoffHz(1000.0f);     // clamp para 200
    if (!(dc.sampleRate >= 1.0f && dc.sampleRate <= 384000.0f)) {
        return fail("DCBlocker sampleRate clamp out of range");
    }
    if (!(dc.cutoffHz >= 1.0f && dc.cutoffHz <= 200.0f)) {
        return fail("DCBlocker cutoff clamp out of range");
    }
    if (!(dc.r >= 0.0f && dc.r <= 0.99999f)) {
        return fail("DCBlocker pole r should remain in [0, 0.99999]");
    }

    dc.reset();
    float y = 0.0f;
    for (int i = 0; i < 5000; ++i) {
        y = dc.process(1.0f);
    }
    if (!std::isfinite(y) || std::fabs(y) > 0.05f) {
        return fail("DCBlocker should attenuate DC input toward zero");
    }

    const float yDecay = dc.process(0.0f);
    if (!std::isfinite(yDecay)) {
        return fail("DCBlocker should remain finite during decay");
    }

    // Nonlinear feedback helpers: polynomial clip should be odd, finite and bounded.
    const float clippedPos = softClipPolynomial(4.0f);
    const float clippedNeg = softClipPolynomial(-4.0f);
    if (!std::isfinite(clippedPos) || !std::isfinite(clippedNeg) || !nearlyEqual(clippedPos, -clippedNeg, 1.0e-6f) || clippedPos > 1.01f) {
        return fail("softClipPolynomial should remain finite, odd and bounded");
    }

    EnvelopeFollowerLimiter limiter;
    limiter.configure(48000.0f);
    float limited = 0.0f;
    for (int i = 0; i < 256; ++i) {
        limited = processDrivenSoftClipLimiter(2.0f, limiter, 8.0f, 1.0f, 0.25f);
    }
    if (!std::isfinite(limited) || std::fabs(limited) >= 2.0f) {
        return fail("feedback nonlinear limiter should reduce a sustained hot signal");
    }

    std::cout << "test_filters: OK\n";
    return 0;
}
