#include "parameter_bridge.h"

namespace orbit::embedded {

ParameterBridge::ParameterBridge() : publishedVersion_(0), lastConsumedVersion_(0) {
    slots_[0] = AudioParams{};
    slots_[1] = AudioParams{};
}

void ParameterBridge::publish(const AudioParams& params) {
    const uint32_t nextVersion = publishedVersion_.load(std::memory_order_relaxed) + 1;
    slots_[nextVersion & 1u] = params;
    publishedVersion_.store(nextVersion, std::memory_order_release);
}

bool ParameterBridge::consumeIfUpdated(AudioParams& outParams) {
    const uint32_t version = publishedVersion_.load(std::memory_order_acquire);
    if (version == lastConsumedVersion_) {
        return false;
    }

    outParams = slots_[version & 1u];
    lastConsumedVersion_ = version;
    return true;
}

} // namespace orbit::embedded
