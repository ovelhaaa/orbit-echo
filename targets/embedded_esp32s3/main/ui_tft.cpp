#include "ui_tft.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace orbit::embedded {
namespace {
constexpr const char* kTag = "ui_tft";
}

UiTft::~UiTft() {
    stop();
}

bool UiTft::start(const Config& config, UiTickCallback callback, void* userData, uint8_t* framebuffer) {
    if (running_.load(std::memory_order_acquire)) {
        return true;
    }

    config_ = config;
    callback_ = callback;
    userData_ = userData;
    framebuffer_ = framebuffer;

    if (!stoppedSignal_) {
        stoppedSignal_ = xSemaphoreCreateBinary();
        if (!stoppedSignal_) {
            ESP_LOGE(kTag, "Falha ao criar semáforo de sincronização da UI");
            return false;
        }
    }

    running_.store(true, std::memory_order_release);

    BaseType_t ok = xTaskCreatePinnedToCore(taskEntry, "ui_tft_task", config_.stackBytes, this,
                                             config_.priority, &taskHandle_, config_.core);

    if (ok != pdPASS) {
        running_.store(false, std::memory_order_release);
        taskHandle_ = nullptr;
        ESP_LOGE(kTag, "Falha ao criar task de UI");
        return false;
    }

    return true;
}

void UiTft::stop() {
    if (!running_.load(std::memory_order_acquire)) {
        if (stoppedSignal_) {
            vSemaphoreDelete(stoppedSignal_);
            stoppedSignal_ = nullptr;
        }
        return;
    }

    running_.store(false, std::memory_order_release);

    if (taskHandle_ && stoppedSignal_) {
        xSemaphoreTake(stoppedSignal_, portMAX_DELAY);
    }

    taskHandle_ = nullptr;

    if (stoppedSignal_) {
        vSemaphoreDelete(stoppedSignal_);
        stoppedSignal_ = nullptr;
    }
}

void UiTft::taskEntry(void* ctx) {
    static_cast<UiTft*>(ctx)->taskLoop();
}

void UiTft::taskLoop() {
    const TickType_t periodTicks = pdMS_TO_TICKS(config_.refreshPeriodMs);
    TickType_t lastWake = xTaskGetTickCount();

    while (running_.load(std::memory_order_acquire)) {
        if (callback_) {
            callback_(userData_);
        }
        vTaskDelayUntil(&lastWake, periodTicks);
    }

    taskHandle_ = nullptr;
    if (stoppedSignal_) {
        xSemaphoreGive(stoppedSignal_);
    }
    vTaskDelete(nullptr);
}

} // namespace orbit::embedded
