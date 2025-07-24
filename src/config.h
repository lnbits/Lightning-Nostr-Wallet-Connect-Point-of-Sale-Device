/**
 * @file config.h
 * @brief System Configuration Constants and Definitions
 * 
 * Contains all system-wide configuration constants, including:
 * - Display and graphics settings for WT32-SC01
 * - WiFi connection parameters and timeouts
 * - LVGL buffer configurations
 * - Screen state definitions
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/**
 * @defgroup DisplayConfig Display Configuration
 * @brief Display and graphics-related configuration constants
 * @{
 */
#define LGFX_AUTODETECT // Autodetect board
#define LGFX_USE_V1     // set to use new version of library

/** @brief Screen width in pixels (portrait mode) */
static const uint16_t SCREEN_WIDTH = 320;   

/** @brief Screen height in pixels (portrait mode) */
static const uint16_t SCREEN_HEIGHT = 480;

/** @brief LVGL buffer size in pixels */
static const uint16_t BUFFER_SIZE = SCREEN_WIDTH * 30;
/** @} */

/**
 * @defgroup WiFiConfig WiFi Configuration
 * @brief WiFi connection and credential storage settings
 * @{
 */
/** @brief Preferences namespace for WiFi credentials */
#define WIFI_PREFERENCES_NAMESPACE "wifi-creds"

/** @brief Preferences key for WiFi SSID */
#define WIFI_SSID_KEY "ssid"

/** @brief Preferences key for WiFi password */
#define WIFI_PASSWORD_KEY "password"

/** @brief WiFi connection timeout in attempts (each 500ms) */
#define WIFI_CONNECTION_TIMEOUT 30
/** @} */

/**
 * @defgroup ScreenStates Screen State Management
 * @brief Screen navigation and state definitions
 * @{
 */

/**
 * @brief Screen state enumeration
 * 
 * Defines the different screens/modes available in the UI
 */
typedef enum {
  SCREEN_KEYPAD,        /**< Main keypad screen for amount entry */
  SCREEN_SETTINGS,      /**< Settings configuration screen */
  SCREEN_WIFI,          /**< WiFi network selection screen */
  SCREEN_WIFI_PASSWORD  /**< WiFi password entry screen */
} screen_state_t;
/** @} */

#endif // CONFIG_H 