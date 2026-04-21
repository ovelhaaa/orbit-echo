#include "audio_engine_esp32.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace orbit::embedded {
namespace {
constexpr const char* kTag = "audio_engine";

#ifndef I2S_MCLK_MULTIPLE_DEFAULT
#define I2S_MCLK_MULTIPLE_DEFAULT I2S_MCLK_MULTIPLE_256
#endif
}

AudioEngineEsp32::AudioEngineEsp32() = default;

AudioEngineEsp32::~AudioEngineEsp32() {
    deinit();
}

bool AudioEngineEsp32::init(const Config& config, AudioCallback callback, void* userData) {
    if (initialized_) {
        return true;
    }

    config_ = config;
    callback_ = callback;
    userData_ = userData;

    i2s_mode_t mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER);
    if (config_.enableTx) {
        mode = static_cast<i2s_mode_t>(mode | I2S_MODE_TX);
    }
    if (config_.enableRx) {
        mode = static_cast<i2s_mode_t>(mode | I2S_MODE_RX);
    }

    const i2s_config_t i2sCfg = {
        .mode = mode,
        .sample_rate = config_.sampleRate,
        .bits_per_sample = static_cast<i2s_bits_per_sample_t>(config_.bitsPerSample),
        .channel_format = config_.channelFormat,
        .communication_format = config_.commFormat,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = config_.dmaBufferCount,
        .dma_buf_len = config_.dmaBufferFrames,
        .use_apll = config_.useApll,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
        .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
    };

    if (i2s_driver_install(config_.port, &i2sCfg, 0, nullptr) != ESP_OK) {
        ESP_LOGE(kTag, "Falha ao instalar driver I2S");
        return false;
    }

    i2s_pin_config_t pinCfg = {
        .bck_io_num = config_.bclkGpio,
        .ws_io_num = config_.wsGpio,
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .data_out_num = config_.enableTx ? config_.doutGpio : I2S_PIN_NO_CHANGE,
        .data_in_num = config_.enableRx ? config_.dinGpio : I2S_PIN_NO_CHANGE,
    };

    if (i2s_set_pin(config_.port, &pinCfg) != ESP_OK) {
        ESP_LOGE(kTag, "Falha ao configurar pinos I2S");
        i2s_driver_uninstall(config_.port);
        return false;
    }

    interleavedSamplesPerBlock_ = static_cast<size_t>(config_.dmaBufferFrames) * 2u;
    const size_t bytes = interleavedSamplesPerBlock_ * sizeof(int32_t);

    if (config_.enableRx) {
        rxBuffer_ = static_cast<int32_t*>(heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    txBuffer_ = static_cast<int32_t*>(heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    if (!txBuffer_ || (config_.enableRx && !rxBuffer_)) {
        ESP_LOGE(kTag, "Falha ao alocar buffers críticos em SRAM interna");
        deinit();
        return false;
    }

    stoppedSignal_ = xSemaphoreCreateBinary();
    if (!stoppedSignal_) {
        ESP_LOGE(kTag, "Falha ao criar semáforo de sincronização da task de áudio");
        deinit();
        return false;
    }

    initialized_ = true;
    return true;
}

bool AudioEngineEsp32::start() {
    if (!initialized_ || running_) {
        return initialized_;
    }

    running_ = true;
    BaseType_t ok = xTaskCreatePinnedToCore(audioTaskEntry, "audio_i2s_task", 4096, this,
                                             configMAX_PRIORITIES - 2, &taskHandle_, 0);
    if (ok != pdPASS) {
        running_ = false;
        taskHandle_ = nullptr;
        ESP_LOGE(kTag, "Falha ao criar task de audio");
        return false;
    }

    return true;
}

void AudioEngineEsp32::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (taskHandle_ && stoppedSignal_) {
        xSemaphoreTake(stoppedSignal_, portMAX_DELAY);
    }

    taskHandle_ = nullptr;
}

void AudioEngineEsp32::deinit() {
    stop();
    if (initialized_) {
        i2s_driver_uninstall(config_.port);
    }

    if (stoppedSignal_) {
        vSemaphoreDelete(stoppedSignal_);
        stoppedSignal_ = nullptr;
    }

    if (rxBuffer_) {
        heap_caps_free(rxBuffer_);
        rxBuffer_ = nullptr;
    }
    if (txBuffer_) {
        heap_caps_free(txBuffer_);
        txBuffer_ = nullptr;
    }

    initialized_ = false;
}

void AudioEngineEsp32::audioTaskEntry(void* ctx) {
    static_cast<AudioEngineEsp32*>(ctx)->audioTaskLoop();
}

void AudioEngineEsp32::audioTaskLoop() {
    const size_t bytesPerBlock = interleavedSamplesPerBlock_ * sizeof(int32_t);
    while (running_) {
        size_t bytesRead = 0;
        if (config_.enableRx) {
            i2s_read(config_.port, rxBuffer_, bytesPerBlock, &bytesRead, pdMS_TO_TICKS(20));
        }

        size_t frames = (bytesRead / sizeof(int32_t)) / 2u;
        if (!config_.enableRx) {
            frames = static_cast<size_t>(config_.dmaBufferFrames);
        }
        if (callback_) {
            callback_(userData_, config_.enableRx ? rxBuffer_ : nullptr, txBuffer_, frames);
        }

        if (config_.enableTx) {
            size_t bytesWritten = 0;
            i2s_write(config_.port, txBuffer_, frames * 2u * sizeof(int32_t), &bytesWritten, pdMS_TO_TICKS(20));
            (void)bytesWritten;
        }
    }

    taskHandle_ = nullptr;
    if (stoppedSignal_) {
        xSemaphoreGive(stoppedSignal_);
    }
    vTaskDelete(nullptr);
}

} // namespace orbit::embedded
