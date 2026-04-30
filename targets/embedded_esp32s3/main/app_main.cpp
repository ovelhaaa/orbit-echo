#include "audio_engine_esp32.h"
#include "audio_source.h"
#include "board_config.h"
#include "parameter_bridge.h"
#include "ui_tft.h"
#include "ui_app.h"

#include "core/include/orbit_delay_core.h"

#include <algorithm>
#include <cstdint>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace orbit::embedded {
namespace {
constexpr const char* kTag = "orbit_app";
// 0.5s @ 48kHz: mantém buffers críticos em SRAM interna com margem de heap no ESP32-S3.
constexpr uint32_t kMaxDelaySamples = 24000;

float* allocateDelayBuffer(size_t samples) {
    const size_t bytes = sizeof(float) * samples;
    float* buffer = static_cast<float*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (buffer) {
        ESP_LOGI(kTag, "Delay buffer alocado em PSRAM (%u bytes)", static_cast<unsigned>(bytes));
        return buffer;
    }

    buffer = static_cast<float*>(heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (buffer) {
        ESP_LOGW(kTag, "Fallback: delay buffer alocado em SRAM interna (%u bytes)", static_cast<unsigned>(bytes));
    }
    return buffer;
}

struct AppContext {
    dsp::OrbitDelayCore core;
    ParameterBridge params;
    AudioEngineEsp32 audio;
    UiTft ui;

    AudioSourceType sourceType = AudioSourceType::ExternalI2s;
    AudioSourceType activeSourceType = AudioSourceType::ExternalI2s;
    I2sInputSource externalI2sSource;
    InternalTestTriangleSource internalTestSource;
    void* activeSource = &externalI2sSource;

    float* delayBufferL = nullptr;
    float* delayBufferR = nullptr;

    // Não crítico: pode ficar em PSRAM (UI/assets/logs/cache).
    uint8_t* uiScratch = nullptr;

    ui::UserInterface uiApp;
};

void applyParams(dsp::OrbitDelayCore& core, const AudioParams& p) {
    dsp::OrbitDelayCore::ReadMode coreReadMode = dsp::OrbitDelayCore::ReadMode::Orbit;
    switch (p.readMode) {
        case AudioParams::ReadMode::Accidental:
            coreReadMode = dsp::OrbitDelayCore::ReadMode::AccidentalReverse;
            break;
        case AudioParams::ReadMode::Orbit:
            coreReadMode = dsp::OrbitDelayCore::ReadMode::Orbit;
            break;
        default:
            coreReadMode = dsp::OrbitDelayCore::ReadMode::Orbit;
            break;
    }
    core.setReadMode(coreReadMode);

    core.setOrbit(p.orbit);
    core.setOffsetSamples(p.offsetSamples);
    core.setStereoSpread(p.stereoSpread);
    core.setFeedback(p.feedback);
    core.setMix(p.mix);
    core.setInputGain(p.inputGain);
    core.setOutputGain(p.outputGain);
    core.setToneHz(p.toneHz);
    core.setSmearAmount(p.smearAmount);
    core.setDiffuserStages(p.diffuserStages);
    core.setDcBlockEnabled(p.dcBlockEnabled);
}

template <typename TSource>
void processWithSource(AppContext* app, TSource& source, int32_t* outInterleaved, size_t frames) {
    for (size_t i = 0; i < frames; ++i) {
        float inL = 0.0f;
        float inR = 0.0f;
        source.renderFrame(i, inL, inR);

        float outL = 0.0f;
        float outR = 0.0f;
        app->core.processSampleStereo(inL, inR, outL, outR);

        const float clampedL = std::clamp(outL, -1.0f, 1.0f);
        const float clampedR = std::clamp(outR, -1.0f, 1.0f);
        outInterleaved[i * 2] = static_cast<int32_t>(clampedL * 2147483647.0f);
        outInterleaved[i * 2 + 1] = static_cast<int32_t>(clampedR * 2147483647.0f);
    }
}

void updateActiveSource(AppContext* app) {
    if (app->activeSourceType == app->sourceType) {
        return;
    }

    app->activeSourceType = app->sourceType;
    switch (app->sourceType) {
        case AudioSourceType::InternalTest:
            app->activeSource = &app->internalTestSource;
            break;
        case AudioSourceType::ExternalI2s:
        default:
            app->externalI2sSource.reset();
            app->activeSource = &app->externalI2sSource;
            break;
    }
}

void audioCallback(void* userData, const int32_t* inInterleaved, int32_t* outInterleaved, size_t frames) {
    auto* app = static_cast<AppContext*>(userData);

    AudioParams params;
    if (app->params.consumeIfUpdated(params)) {
        applyParams(app->core, params);
    }

    updateActiveSource(app);

    switch (app->activeSourceType) {
        case AudioSourceType::InternalTest: {
            auto* source = static_cast<InternalTestTriangleSource*>(app->activeSource);
            source->prepare(inInterleaved);
            processWithSource(app, *source, outInterleaved, frames);
            break;
        }
        case AudioSourceType::ExternalI2s:
        default: {
            auto* source = static_cast<I2sInputSource*>(app->activeSource);
            source->prepare(inInterleaved);
            processWithSource(app, *source, outInterleaved, frames);
            break;
        }
    }
}

void uiTick(void* userData) {
    auto* app = static_cast<AppContext*>(userData);
    app->uiApp.tick(app->params);
}

} // namespace
} // namespace orbit::embedded

extern "C" void app_main(void) {
    using namespace orbit::embedded;

    static AppContext app;

    // Delay line pode usar PSRAM para reduzir pressão de SRAM interna.
    app.delayBufferL = allocateDelayBuffer(kMaxDelaySamples);
    app.delayBufferR = allocateDelayBuffer(kMaxDelaySamples);

    // Scratch da UI usada pelo display: precisa ser DMA-capable em SRAM interna.
    const size_t fbSize = board::tft::kWidth * board::tft::kHeight * 2;
    app.uiScratch = static_cast<uint8_t*>(heap_caps_malloc(fbSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));

    if (!app.delayBufferL || !app.delayBufferR || !app.uiScratch) {
        ESP_LOGE(kTag, "Falha ao alocar buffers (SRAM interna/DMA)");
        return;
    }

    app.core.attachBuffers(app.delayBufferL, app.delayBufferR, kMaxDelaySamples);
    app.core.reset(static_cast<float>(board::audio::kSampleRate));

    AudioParams initialParams;
    initialParams.readMode = AudioParams::ReadMode::Accidental;
    initialParams.dcBlockEnabled = true;
    app.params.publish(initialParams);

    app.internalTestSource.setSampleRate(static_cast<float>(board::audio::kSampleRate));
    app.internalTestSource.setFrequencyHz(220.0f);
    app.internalTestSource.setLevel(0.15f);
    app.externalI2sSource.reset();
    app.externalI2sSource.setSampleAlign(SampleAlign::Left24In32);
    app.externalI2sSource.setStereoOrder(StereoOrder::RightLeft);

    AudioEngineEsp32::Config audioCfg;
    audioCfg.sampleRate = board::audio::kSampleRate;
    audioCfg.enableRx = true;
    audioCfg.enableTx = true;
    audioCfg.mclkGpio = board::audio::i2s::kMclkGpio;
    audioCfg.fixedMclkHz = board::audio::kSampleRate * 256;
    audioCfg.useApll = true;

    if (!app.audio.init(audioCfg, audioCallback, &app) || !app.audio.start()) {
        ESP_LOGE(kTag, "Falha ao iniciar engine de áudio");
        return;
    }

    UiTft::Config uiCfg;
    uiCfg.refreshPeriodMs = board::ui::kRefreshPeriodMs;
    uiCfg.core = board::ui::kTaskCore;
    uiCfg.priority = board::ui::kTaskPriority;

    if (!app.uiApp.init(app.uiScratch)) {
        ESP_LOGE(kTag, "Falha ao inicializar o hardware da UI");
    }

    if (!app.ui.start(uiCfg, uiTick, &app, app.uiScratch)) {
        ESP_LOGW(kTag, "Task da UI não iniciou; áudio segue ativo");
    }

    ESP_LOGI(kTag, "Sistema iniciado: core DSP, áudio I2S+DMA e UI em tasks separadas");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
