#pragma once

#include <cstdint>

namespace orbit::embedded::board {

namespace audio {
constexpr int kSampleRate = 48000;
constexpr int kI2sBitsPerSlot = 32;
// Compatibilidade retroativa: bitsPerSample antigo mapeia para largura de slot I2S.
constexpr int kBitsPerSample = kI2sBitsPerSlot;
constexpr int kDmaBufferCount = 6;
constexpr int kDmaBufferFrames = 128;

namespace i2s {
constexpr int kMclkGpio = 10;
constexpr int kBclkGpio = 11;
constexpr int kLrckGpio = 12;
constexpr int kDoutGpio = 6; // TX mantido no pino legado; RX/MCLK usam pinagem validada do PCM1808.
constexpr int kDinGpio = 13; // DOUT do PCM1808 (entrada RX no ESP32-S3)
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
constexpr int kA = 14;
constexpr int kB = 15;
constexpr int kSwitch = 16;
} // namespace encoder

namespace controls {
constexpr int kBypassButton = 5;
} // namespace controls

namespace ui {
constexpr uint32_t kRefreshPeriodMs = 33;
constexpr int kTaskCore = 1;
constexpr uint32_t kTaskPriority = 2;
} // namespace ui

} // namespace orbit::embedded::board
