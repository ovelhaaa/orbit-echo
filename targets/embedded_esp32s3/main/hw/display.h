#pragma once

#include <cstdint>
#include <string>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "../board_config.h"
#include "esp_log.h"

namespace orbit::embedded::hw {

class Display {
public:
    static constexpr uint16_t kWidth = board::tft::kWidth;
    static constexpr uint16_t kHeight = board::tft::kHeight;
    static constexpr size_t kFrameBufferSize = kWidth * kHeight * 2; // 16-bit color (RGB565)

    Display() = default;

    bool init(uint8_t* buffer) {
        framebuffer_ = buffer;
        if (!framebuffer_) return false;

        // Init SPI bus
        spi_bus_config_t buscfg = {
            .mosi_io_num = board::tft::spi::kMosiGpio,
            .miso_io_num = -1,
            .sclk_io_num = board::tft::spi::kSclkGpio,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = (int)kFrameBufferSize,
            .flags = SPICOMMON_BUSFLAG_MASTER,
            .intr_flags = 0,
        };
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

        esp_lcd_panel_io_handle_t io_handle = nullptr;
        esp_lcd_panel_io_spi_config_t io_config = {
            .cs_gpio_num = board::tft::spi::kCsGpio,
            .dc_gpio_num = board::tft::kDcGpio,
            .spi_mode = 0,
            .pclk_hz = 40 * 1000 * 1000,
            .trans_queue_depth = 10,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .octal_mode = 0,
                .sio_mode = 0,
                .lsb_first = 0,
                .cs_high_active = 0
            }
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

        esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = board::tft::kResetGpio,
            .rgb_endian = LCD_RGB_ENDIAN_RGB,
            .bits_per_pixel = 16,
            .flags = {
                .reset_active_high = 0
            },
            .vendor_config = nullptr
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle_));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle_));
        // Configurações específicas para ST7789 no TTGO T-Display (ou similar 240x135)
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle_, true));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle_, true));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle_, false, true));
        ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle_, 40, 53)); // Offset comum para 240x135

        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle_, true));

        // Backlight
        gpio_config_t bk_gpio_config = {
            .pin_bit_mask = 1ULL << board::tft::kBacklightGpio,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
        gpio_set_level((gpio_num_t)board::tft::kBacklightGpio, 1);

        clear(0x0000);
        update();
        return true;
    }

    void update() {
        if (!panel_handle_ || !framebuffer_) return;
        esp_lcd_panel_draw_bitmap(panel_handle_, 0, 0, kWidth, kHeight, framebuffer_);
    }

    // Mini Graphics Library
    void clear(uint16_t color) {
        fillRect(0, 0, kWidth, kHeight, color);
    }

    void fillRect(int x, int y, int w, int h, uint16_t color) {
        if (!framebuffer_) return;
        uint16_t color_swap = (color >> 8) | (color << 8); // Swap bytes for SPI

        for (int i = 0; i < h; i++) {
            int drawY = y + i;
            if (drawY < 0 || drawY >= kHeight) continue;
            for (int j = 0; j < w; j++) {
                int drawX = x + j;
                if (drawX < 0 || drawX >= kWidth) continue;

                int index = (drawY * kWidth + drawX) * 2;
                framebuffer_[index] = (uint8_t)(color_swap >> 8);
                framebuffer_[index + 1] = (uint8_t)(color_swap & 0xFF);
            }
        }
    }

    // Extremely simple 5x7 font representation for 8 ASCII chars (just enough for POC)
    // In a real scenario we'd use a full font array. Here we just draw rectangles to approximate or implement a basic fallback
    void drawChar(int x, int y, char c, uint16_t color) {
        // Just drawing a block for placeholder if font is not loaded
        fillRect(x, y, 6, 8, color);
    }

    // We will just draw a text representation by rendering blocks, or assume we have a simple draw function
    // For this POC, we will create a simple 5x7 font implementation inline
    void drawText(int x, int y, const char* str, uint16_t color) {
        int cursor_x = x;
        while (*str) {
            drawSimpleChar(cursor_x, y, *str, color);
            cursor_x += 6;
            str++;
        }
    }

private:
    void drawSimplePixel(int x, int y, uint16_t color) {
        if (!framebuffer_ || x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
        uint16_t color_swap = (color >> 8) | (color << 8);
        int index = (y * kWidth + x) * 2;
        framebuffer_[index] = (uint8_t)(color_swap >> 8);
        framebuffer_[index + 1] = (uint8_t)(color_swap & 0xFF);
    }

    // Very basic hardcoded font for UI POC
    void drawSimpleChar(int x, int y, char c, uint16_t color) {
        const uint8_t font5x7[][5] = {
            {0x00, 0x00, 0x00, 0x00, 0x00}, // Space
            {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
            {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
            {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
            {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
            {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
            {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
            {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
            {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
            {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
            {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
            {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
            {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
            {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
            {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
            {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
            {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
            {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
            {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
            {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
            {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
            {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
            {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
            {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
            {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
            {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
            {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
            {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
            {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
            {0x46, 0x49, 0x49, 0x49, 0x31}, // S
            {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
            {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
            {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
            {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
            {0x63, 0x14, 0x08, 0x14, 0x63}, // X
            {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
            {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
            {0x08, 0x08, 0x08, 0x08, 0x08}, // - (index 37)
            {0x00, 0x60, 0x60, 0x00, 0x00}, // . (index 38)
            {0x20, 0x10, 0x08, 0x04, 0x02}, // / (index 39)
            {0x00, 0x00, 0x5F, 0x00, 0x00}, // ! (index 40)
        };

        int idx = 0; // Space
        if (c >= '0' && c <= '9') idx = 1 + (c - '0');
        else if (c >= 'A' && c <= 'Z') idx = 11 + (c - 'A');
        else if (c >= 'a' && c <= 'z') idx = 11 + (c - 'a'); // Map lower to upper
        else if (c == '-') idx = 37;
        else if (c == '.') idx = 38;
        else if (c == '/') idx = 39;
        else if (c == '!') idx = 40;

        for (int col = 0; col < 5; col++) {
            uint8_t line = font5x7[idx][col];
            for (int row = 0; row < 7; row++) {
                if (line & (1 << row)) {
                    drawSimplePixel(x + col, y + row, color);
                }
            }
        }
    }

    esp_lcd_panel_handle_t panel_handle_ = nullptr;
    uint8_t* framebuffer_ = nullptr;
};

// Colors (RGB565)
constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr uint16_t COLOR_WHITE = 0xFFFF;
constexpr uint16_t COLOR_ORBIT_BLUE = 0x041F; // Nice deep blue
constexpr uint16_t COLOR_ACCENT = 0xF800; // Red
constexpr uint16_t COLOR_GREEN = 0x07E0;
constexpr uint16_t COLOR_GRAY = 0x7BEF;

} // namespace orbit::embedded::hw
