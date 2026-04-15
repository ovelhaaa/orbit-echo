#pragma once

#include <cstdint>

namespace orbit::embedded::board {

namespace audio {
constexpr int kSampleRate = 48000;
constexpr int kBitsPerSample = 32;
constexpr int kDmaBufferCount = 6;
constexpr int kDmaBufferFrames = 128;

namespace i2s {
constexpr int kBclkGpio = 4;
constexpr int kLrckGpio = 5;
constexpr int kDoutGpio = 6;
constexpr int kDinGpio = 7; // Opcional (RX/ADC)
} // namespace i2s
} // namespace audio

namespace tft {
constexpr uint16_t kWidth = 240;
constexpr uint16_t kHeight = 135;

namespace spi {
constexpr int kMosiGpio = 35;
constexpr int kSclkGpio = 36;
constexpr int kCsGpio = 37;
} // namespace spi

constexpr int kDcGpio = 38;
constexpr int kResetGpio = 39;
constexpr int kBacklightGpio = 40;
} // namespace tft

namespace encoder {
constexpr int kA = 11;
constexpr int kB = 12;
constexpr int kSwitch = 13;
} // namespace encoder

namespace controls {
constexpr int kBypassButton = 14;
} // namespace controls

namespace ui {
constexpr uint32_t kRefreshPeriodMs = 33;
constexpr int kTaskCore = 1;
constexpr uint32_t kTaskPriority = 2;
} // namespace ui

} // namespace orbit::embedded::board
