#include <zephyr.h>
#include <logging/log.h>
#include <drivers/led_strip.h>
#include <net/wifi_mgmt.h>
#include <net/socket.h>
#include <fs/fs.h>
#include <data/json.h>
#include "config.h"

LOG_MODULE_REGISTER(main);

// LED Control
static const struct device *led_strip;
static uint8_t brightness = DEFAULT_BRIGHTNESS;
static bool power_on = true;
static char current_mode[16] = "solid"; // solid, rainbow, pulse
static uint32_t current_color = DEFAULT_COLOR;

// Filesystem
static struct fs_mount_t littlefs_mnt = {
    .type = FS_LITTLEFS,
    .mnt_point = "/lfs",
    .storage_dev = (void *)FLASH_AREA_ID(storage),
};

// WebSocket
static int web_socket_fd;
static struct sockaddr_in server_addr;

// Persistent Settings
void save_settings(void) {
    struct fs_file_t file;
    fs_file_t_init(&file);

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "{\"power\":%s,\"brightness\":%d,\"mode\":\"%s\",\"color\":%lu}",
             power_on ? "true" : "false", brightness, current_mode, current_color);

    int ret = fs_open(&file, "/lfs/config.json", FS_O_CREATE | FS_O_WRITE);
    if (ret < 0) {
        LOG_ERR("Failed to open file for writing: %d", ret);
        return;
    }

    ret = fs_write(&file, buffer, strlen(buffer));
    if (ret < 0) {
        LOG_ERR("Failed to write to file: %d", ret);
    }

    fs_close(&file);
}

void load_settings(void) {
    struct fs_file_t file;
    fs_file_t_init(&file);

    int ret = fs_open(&file, "/lfs/config.json", FS_O_READ);
    if (ret < 0) {
        LOG_ERR("Failed to open file for reading: %d", ret);
        return;
    }

    char buffer[128];
    ret = fs_read(&file, buffer, sizeof(buffer));
    if (ret < 0) {
        LOG_ERR("Failed to read from file: %d", ret);
        fs_close(&file);
        return;
    }

    fs_close(&file);

    json_obj_parse(buffer, strlen(buffer), json_descr, ARRAY_SIZE(json_descr), &settings);
    power_on = settings.power;
    brightness = settings.brightness;
    strncpy(current_mode, settings.mode, sizeof(current_mode) - 1);
    current_color = settings.color;
}

// LED Control Logic
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

    // Initialize LittleFS
    if (fs_mount(&littlefs_mnt) != 0) {
        LOG_ERR("Failed to mount LittleFS");
        return;
    }
    LOG_INF("LittleFS mounted successfully");

    // Load settings
    load_settings();

    // Initialize LED Strip
    led_strip = device_get_binding(DT_LABEL(DT_INST(0, worldsemi_ws2812)));
    if (!led_strip) {
        LOG_ERR("LED strip device not found");
        return;
    }

    // Connect to WiFi
    struct net_if *iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("No network interface found");
        return;
    }

    struct wifi_connect_req_params params = {
        .ssid = WIFI_SSID,
        .ssid_length = strlen(WIFI_SSID),
        .psk = WIFI_PASSWORD,
        .psk_length = strlen(WIFI_PASSWORD),
        .channel = WIFI_CHANNEL_ANY,
        .security = WIFI_SECURITY_TYPE_PSK,
    };

    if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params))) {
        LOG_ERR("Failed to connect to WiFi");
        return;
    }

    LOG_INF("WiFi connected!");

    // Main loop
    while (1) {
        apply_led_settings();
        k_sleep(K_MSEC(20));
    }
}
