#include "parameter_bridge.h"

namespace orbit::embedded {

namespace {
constexpr uint32_t kMaxSnapshotAttempts = 4u;
} // namespace

ParameterBridge::ParameterBridge()
    : publishedVersion_(0u),
      lastConsumedVersion_(0u),
      orbit_(0.5f),
      offsetSamples_(1200.0f),
      stereoSpread_(0.0f),
      feedback_(0.35f),
      mix_(0.35f),
      inputGain_(1.0f),
      outputGain_(1.0f),
      toneHz_(8000.0f),
      smearAmount_(0.0f),
      diffuserStages_(2u),
      dcBlockEnabled_(true),
      readMode_(AudioParams::ReadMode::Accidental),
      sourceType_(AudioSourceType::InternalTest) {
}

void ParameterBridge::publish(const AudioParams& params) {
    const uint32_t baseVersion = publishedVersion_.load(std::memory_order_relaxed);
    publishedVersion_.store(baseVersion + 1u, std::memory_order_release); // writer ativo (ímpar)

    orbit_.store(params.orbit, std::memory_order_relaxed);
    offsetSamples_.store(params.offsetSamples, std::memory_order_relaxed);
    stereoSpread_.store(params.stereoSpread, std::memory_order_relaxed);
    feedback_.store(params.feedback, std::memory_order_relaxed);
    mix_.store(params.mix, std::memory_order_relaxed);
    inputGain_.store(params.inputGain, std::memory_order_relaxed);
    outputGain_.store(params.outputGain, std::memory_order_relaxed);
    toneHz_.store(params.toneHz, std::memory_order_relaxed);
    smearAmount_.store(params.smearAmount, std::memory_order_relaxed);
    diffuserStages_.store(params.diffuserStages, std::memory_order_relaxed);
    dcBlockEnabled_.store(params.dcBlockEnabled, std::memory_order_relaxed);
    readMode_.store(params.readMode, std::memory_order_relaxed);
    sourceType_.store(params.sourceType, std::memory_order_relaxed);
    publishedVersion_.store(baseVersion + 2u, std::memory_order_release); // snapshot estável (par)
}

bool ParameterBridge::consumeIfUpdated(AudioParams& outParams) {
    for (uint32_t attempt = 0; attempt < kMaxSnapshotAttempts; ++attempt) {
        const uint32_t beginVersion = publishedVersion_.load(std::memory_order_acquire);
        if ((beginVersion & 1u) != 0u) {
            continue;
        }
        if (beginVersion == lastConsumedVersion_) {
            return false;
        }

        AudioParams snapshot;
        snapshot.orbit = orbit_.load(std::memory_order_relaxed);
        snapshot.offsetSamples = offsetSamples_.load(std::memory_order_relaxed);
        snapshot.stereoSpread = stereoSpread_.load(std::memory_order_relaxed);
        snapshot.feedback = feedback_.load(std::memory_order_relaxed);
        snapshot.mix = mix_.load(std::memory_order_relaxed);
        snapshot.inputGain = inputGain_.load(std::memory_order_relaxed);
        snapshot.outputGain = outputGain_.load(std::memory_order_relaxed);
        snapshot.toneHz = toneHz_.load(std::memory_order_relaxed);
        snapshot.smearAmount = smearAmount_.load(std::memory_order_relaxed);
        snapshot.diffuserStages = diffuserStages_.load(std::memory_order_relaxed);
        snapshot.dcBlockEnabled = dcBlockEnabled_.load(std::memory_order_relaxed);
        snapshot.readMode = readMode_.load(std::memory_order_relaxed);
        snapshot.sourceType = sourceType_.load(std::memory_order_relaxed);

        std::atomic_thread_fence(std::memory_order_acquire);
        const uint32_t endVersion = publishedVersion_.load(std::memory_order_relaxed);
        if (beginVersion == endVersion) {
            outParams = snapshot;
            lastConsumedVersion_ = endVersion;
            return true;
        }
    }

    return false;
}

} // namespace orbit::embedded
