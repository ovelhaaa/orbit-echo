#pragma once

#include <atomic>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace orbit::embedded {

class UiTft {
public:
    struct Config {
        uint32_t refreshPeriodMs = 33;
        uint32_t stackWords = 4096;
        uint32_t priority = 3;
        int core = 1;
    };

    using UiTickCallback = void (*)(void* userData);

    UiTft() = default;
    ~UiTft();

    bool start(const Config& config, UiTickCallback callback, void* userData, uint8_t* framebuffer);
    void stop();

    uint8_t* getFramebuffer() const { return framebuffer_; }

private:
    static void taskEntry(void* ctx);
    void taskLoop();

    Config config_{};
    UiTickCallback callback_ = nullptr;
    void* userData_ = nullptr;
    uint8_t* framebuffer_ = nullptr;
    TaskHandle_t taskHandle_ = nullptr;
    SemaphoreHandle_t stoppedSignal_ = nullptr;
    std::atomic_bool running_{false};
};

} // namespace orbit::embedded
