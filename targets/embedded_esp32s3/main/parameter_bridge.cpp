#include "parameter_bridge.h"

namespace orbit::embedded {

namespace {
constexpr uint32_t kMaxSnapshotAttempts = 4u;
} // namespace

ParameterBridge::ParameterBridge() : latest_(AudioParams{}), sequence_(0u), lastConsumedSequence_(0u) {
}

void ParameterBridge::publish(const AudioParams& params) {
    sequence_.fetch_add(1u, std::memory_order_acq_rel); // odd -> writer ativo
    latest_ = params;
    sequence_.fetch_add(1u, std::memory_order_release); // even -> snapshot estável
}

bool ParameterBridge::consumeIfUpdated(AudioParams& outParams) {
    for (uint32_t attempt = 0; attempt < kMaxSnapshotAttempts; ++attempt) {
        const uint32_t beginSeq = sequence_.load(std::memory_order_acquire);
        if ((beginSeq & 1u) != 0u) {
            continue;
        }
        if (beginSeq == lastConsumedSequence_) {
            return false;
        }

        const AudioParams snapshot = latest_;
        const uint32_t endSeq = sequence_.load(std::memory_order_acquire);
        if (beginSeq == endSeq && (endSeq & 1u) == 0u) {
            outParams = snapshot;
            lastConsumedSequence_ = endSeq;
            return true;
        }
    }

    return false;
}

} // namespace orbit::embedded
