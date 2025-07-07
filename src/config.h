#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// Display configuration
#define LGFX_AUTODETECT // Autodetect board
#define LGFX_USE_V1     // set to use new version of library

// Screen resolution for LVGL
static const uint16_t SCREEN_WIDTH = 320;   // Portrait mode
static const uint16_t SCREEN_HEIGHT = 480;

// Buffer size for LVGL
static const uint16_t BUFFER_SIZE = SCREEN_WIDTH * 30;  // Double the buffer size

// WiFi credentials storage
#define WIFI_PREFERENCES_NAMESPACE "wifi-creds"
#define WIFI_SSID_KEY "ssid"
#define WIFI_PASSWORD_KEY "password"

// WiFi connection timeout (in attempts, each 500ms)
#define WIFI_CONNECTION_TIMEOUT 30

// Screen state management
typedef enum {
  SCREEN_KEYPAD,
  SCREEN_SETTINGS,
  SCREEN_WIFI,
  SCREEN_WIFI_PASSWORD
} screen_state_t;

#endif // CONFIG_H 