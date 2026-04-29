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
constexpr int kCsGpio = 7;
} // namespace spi

constexpr int kDcGpio = 39;
constexpr int kResetGpio = 40;
constexpr int kBacklightGpio = 45;
constexpr int kPowerGpio = 21; // TFT_I2C_POWER on Adafruit ESP32-S3 TFT Feather
} // namespace tft

namespace encoder {
// Board rev note:
// Encoder intentionally uses GPIO14/15/16 on the current TFT Feather wiring.
// This repurposes default JTAG pins and must only be used on hardware revisions
// that do not require external JTAG and do not populate an external 32kHz crystal
// on GPIO15/16.
constexpr int kA = 14;
constexpr int kB = 15;
constexpr int kSwitch = 16;
constexpr bool kPresent = true; // Set true on board revisions with populated rotary encoder.
} // namespace encoder

namespace controls {
constexpr int kBypassButton = 9;
} // namespace controls

// Guard against accidental overlap with active audio I2S signals.
static_assert(encoder::kA != audio::i2s::kBclkGpio && encoder::kA != audio::i2s::kLrckGpio &&
                  encoder::kA != audio::i2s::kDoutGpio && encoder::kA != audio::i2s::kDinGpio &&
                  encoder::kA != audio::i2s::kMclkGpio,
              "Encoder GPIO A conflicts with I2S pin mapping.");
static_assert(encoder::kB != audio::i2s::kBclkGpio && encoder::kB != audio::i2s::kLrckGpio &&
                  encoder::kB != audio::i2s::kDoutGpio && encoder::kB != audio::i2s::kDinGpio &&
                  encoder::kB != audio::i2s::kMclkGpio,
              "Encoder GPIO B conflicts with I2S pin mapping.");
static_assert(encoder::kSwitch != audio::i2s::kBclkGpio && encoder::kSwitch != audio::i2s::kLrckGpio &&
                  encoder::kSwitch != audio::i2s::kDoutGpio && encoder::kSwitch != audio::i2s::kDinGpio &&
                  encoder::kSwitch != audio::i2s::kMclkGpio,
              "Encoder switch GPIO conflicts with I2S pin mapping.");

namespace ui {
constexpr uint32_t kRefreshPeriodMs = 33;
constexpr int kTaskCore = 1;
constexpr uint32_t kTaskPriority = 2;
} // namespace ui

} // namespace orbit::embedded::board
