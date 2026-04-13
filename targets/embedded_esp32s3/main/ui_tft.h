#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"
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

    bool start(const Config& config, UiTickCallback callback, void* userData);
    void stop();

private:
    static void taskEntry(void* ctx);
    void taskLoop();

    Config config_{};
    UiTickCallback callback_ = nullptr;
    void* userData_ = nullptr;
    TaskHandle_t taskHandle_ = nullptr;
    bool running_ = false;
};

} // namespace orbit::embedded
