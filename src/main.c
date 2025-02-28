#include <zephyr.h>
#include <drivers/led_strip.h>
#include <logging/log.h>
#include "config.h"

LOG_MODULE_REGISTER(main);

// LED Control
static const struct device *led_strip;
static uint8_t brightness = DEFAULT_BRIGHTNESS;
static bool power_on = true;
static char current_mode[16] = "solid"; // solid, rainbow, pulse
static uint32_t current_color = DEFAULT_COLOR;

// Initialize LED Strip
void init_led_strip(void) {
    led_strip = DEVICE_DT_GET(DT_ALIAS(led_strip));
    if (!device_is_ready(led_strip)) {
        LOG_ERR("LED strip device not ready");
        return;
    }
    LOG_INF("LED strip initialized");
}

// Apply LED Settings
void apply_led_settings(void) {
    struct led_rgb pixels[NUM_LEDS];

    if (!power_on) {
        memset(pixels, 0, sizeof(pixels));
    } else if (strcmp(current_mode, "solid") == 0) {
        uint8_t r = (current_color >> 16) & 0xFF;
        uint8_t g = (current_color >> 8) & 0xFF;
        uint8_t b = current_color & 0xFF;
        for (int i = 0; i < NUM_LEDS; i++) {
            pixels[i].r = r;
            pixels[i].g = g;
            pixels[i].b = b;
        }
    } else if (strcmp(current_mode, "rainbow") == 0) {
        static uint8_t hue = 0;
        for (int i = 0; i < NUM_LEDS; i++) {
            pixels[i] = hsv_to_rgb((hue + (i * 256 / NUM_LEDS)) % 256, 255, 255);
        }
        hue++;
    } else if (strcmp(current_mode, "pulse") == 0) {
        static uint8_t brightness_mod = 0;
        static int direction = 1;
        brightness_mod += direction;
        if (brightness_mod == 255 || brightness_mod == 0) direction = -direction;
        uint8_t r = (current_color >> 16) & 0xFF;
        uint8_t g = (current_color >> 8) & 0xFF;
        uint8_t b = current_color & 0xFF;
        for (int i = 0; i < NUM_LEDS; i++) {
            pixels[i].r = r * brightness_mod / 255;
            pixels[i].g = g * brightness_mod / 255;
            pixels[i].b = b * brightness_mod / 255;
        }
    }

    led_strip_update_rgb(led_strip, pixels, NUM_LEDS);
}

// Main Function
void main(void) {
    LOG_INF("Starting LED Control Application");

    // Initialize LED Strip
    init_led_strip();

    // Main Loop
    while (1) {
        apply_led_settings();
        k_sleep(K_MSEC(20));
    }
}
