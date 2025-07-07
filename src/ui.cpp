#include <Arduino.h>

#include "ui.h"
#include "settings.h"
#include "wifi_manager.h"
#include "display.h"
#include "nwc.h"
#include "app.h"

// Forward declarations for external functions
extern lv_obj_t* wifi_list;
extern bool nwc_initialized;
extern String nwcPairingUrl;

namespace UI {
    // Font definitions
    const lv_font_t* Fonts::FONT_DEFAULT = &lv_font_montserrat_14;
    const lv_font_t* Fonts::FONT_LARGE = &lv_font_montserrat_16;
    const lv_font_t* Fonts::FONT_XLARGE = &lv_font_montserrat_24;
    const lv_font_t* Fonts::FONT_SMALL = &lv_font_montserrat_12;
    
    // Current screen state
    static screen_state_t current_screen = SCREEN_KEYPAD;
    
    // Global UI elements
    static lv_obj_t* display_label = NULL;
    static lv_obj_t* wifi_list = NULL;
    static lv_obj_t* qr_canvas = NULL;
    static lv_obj_t* invoice_label = NULL;
    static lv_obj_t* invoice_spinner = NULL;
    static lv_obj_t* main_wifi_status_label = NULL;
    
    // Invoice overlay elements
    static lv_obj_t* invoice_overlay = NULL;
    static lv_obj_t* invoice_amount_label = NULL;
    static bool invoice_processing = false;
    
    // Settings screen elements
    static lv_obj_t* shop_name_textarea = NULL;
    static lv_obj_t* ap_password_textarea = NULL;
    static lv_obj_t* shop_name_keyboard = NULL;
    static lv_obj_t* ap_password_keyboard = NULL;
    static lv_obj_t* settings_pin_btn = NULL;
    static lv_obj_t* settings_save_btn = NULL;
    
    void init() {
        // UI is initialized through Display::init()
        // This function can be used for additional UI setup
        current_screen = SCREEN_KEYPAD;
        loadScreen(SCREEN_KEYPAD);
    }
    
    void cleanup() {
        cleanupGlobalPointers();
    }
    
    void cleanupGlobalPointers() {
        display_label = NULL;
        wifi_list = NULL;
        qr_canvas = NULL;
        invoice_label = NULL;
        invoice_spinner = NULL;
        main_wifi_status_label = NULL;
        invoice_overlay = NULL;
        invoice_amount_label = NULL;
        shop_name_textarea = NULL;
        ap_password_textarea = NULL;
        shop_name_keyboard = NULL;
        ap_password_keyboard = NULL;
        settings_pin_btn = NULL;
        settings_save_btn = NULL;
    }
    
    void loadScreen(screen_state_t screen) {
        current_screen = screen;
        
        // Use smoother screen clearing
        lv_obj_clean(lv_scr_act());
        cleanupGlobalPointers();
        
        // Add a small delay to allow the clear to complete
        lv_timer_handler();
        
        switch (screen) {
            case SCREEN_KEYPAD:
                createKeypadScreen();
                break;
            case SCREEN_SETTINGS:
                createSettingsScreen();
                break;
            case SCREEN_WIFI:
                createWiFiScreen();
                break;
            case SCREEN_WIFI_PASSWORD:
                // This is handled by createWiFiPasswordScreen(ssid)
                break;
            case SCREEN_SETTINGS_SUB:
                createSettingsSubScreen();
                break;
            case SCREEN_INFO:
                createInfoScreen();
                break;
        }
        
        // Force an immediate refresh after screen creation
        lv_timer_handler();
    }
    
    void createKeypadScreen() {
        // Status Bar
        main_wifi_status_label = lv_label_create(lv_scr_act());
        lv_obj_align(main_wifi_status_label, LV_ALIGN_TOP_LEFT, 10, 5);
        lv_label_set_text(main_wifi_status_label, LV_SYMBOL_WIFI " Not Connected");
        lv_obj_set_style_text_color(main_wifi_status_label, lv_color_hex(0x9E9E9E), 0);

        // Relay status label
        lv_obj_t* relay_status_label = lv_label_create(lv_scr_act());
        lv_obj_align(relay_status_label, LV_ALIGN_TOP_RIGHT, -10, 5);
        lv_label_set_text(relay_status_label, "Relay: Disconnected");
        lv_obj_set_style_text_color(relay_status_label, lv_color_hex(0x9E9E9E), 0);
        
        // Set reference for NWC module to update
        NWC::setRelayStatusLabel(relay_status_label);

        // Create display area at the top
        lv_obj_t *display_panel = lv_obj_create(lv_scr_act());
        lv_obj_set_size(display_panel, 300, 80);
        lv_obj_set_pos(display_panel, 10, 30);
        lv_obj_set_style_bg_color(display_panel, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_border_width(display_panel, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(display_panel, lv_color_hex(0x666666), LV_PART_MAIN);
        
        display_label = lv_label_create(display_panel);
        lv_label_set_text(display_label, ("0 " + Settings::getCurrency()).c_str());
        lv_obj_set_style_text_color(display_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(display_label, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_align(display_label, LV_ALIGN_CENTER, 0, 0);

        // Keypad layout parameters
        const int num_button_spacing = 5;
        const int num_button_width = (320 - (4 * num_button_spacing)) / 3;
        const int num_button_height = 65;
        const int start_y = 120;
        
        // Create number buttons 1-9
        const char* numbers[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
        int row = 0, col = 0;
        
        for (int i = 0; i < 9; i++) {
            lv_obj_t *btn = lv_btn_create(lv_scr_act());
            lv_obj_set_size(btn, num_button_width, num_button_height);
            lv_obj_set_pos(btn, num_button_spacing + col * (num_button_width + num_button_spacing), 
                           start_y + row * (num_button_height + num_button_spacing));
            
            lv_obj_add_event_cb(btn, keypadEventHandler, LV_EVENT_CLICKED, NULL);
            
            lv_obj_t *label = lv_label_create(btn);
            lv_label_set_text(label, numbers[i]);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_PART_MAIN);
            lv_obj_center(label);

            // Style for number buttons
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x8A2BE2), LV_PART_MAIN);
            lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), LV_PART_MAIN);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x9370DB), LV_STATE_PRESSED);
            lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
            
            col++;
            if (col >= 3) {
                col = 0;
                row++;
            }
        }
        
        // Bottom row: Decimal, 0, Backspace
        int bottom_row_y = start_y + 3 * (num_button_height + num_button_spacing);
        
        // Decimal button (.)
        lv_obj_t *decimal_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(decimal_btn, num_button_width, num_button_height);
        lv_obj_set_pos(decimal_btn, num_button_spacing, bottom_row_y);
        lv_obj_add_event_cb(decimal_btn, keypadEventHandler, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t *decimal_label = lv_label_create(decimal_btn);
        lv_label_set_text(decimal_label, ".");
        lv_obj_set_style_text_font(decimal_label, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_center(decimal_label);
        
        // Style for decimal button
        lv_obj_set_style_bg_color(decimal_btn, lv_color_hex(0x8A2BE2), LV_PART_MAIN);
        lv_obj_set_style_text_color(decimal_btn, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_color(decimal_btn, lv_color_hex(0x9370DB), LV_STATE_PRESSED);
        lv_obj_set_style_text_color(decimal_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);

        // Zero button (0)
        lv_obj_t *zero_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(zero_btn, num_button_width, num_button_height);
        lv_obj_set_pos(zero_btn, num_button_spacing + (num_button_width + num_button_spacing), bottom_row_y);
        lv_obj_add_event_cb(zero_btn, keypadEventHandler, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t *zero_label = lv_label_create(zero_btn);
        lv_label_set_text(zero_label, "0");
        lv_obj_set_style_text_font(zero_label, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_center(zero_label);
        
        // Style for zero button
        lv_obj_set_style_bg_color(zero_btn, lv_color_hex(0x8A2BE2), LV_PART_MAIN);
        lv_obj_set_style_text_color(zero_btn, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_color(zero_btn, lv_color_hex(0x9370DB), LV_STATE_PRESSED);
        lv_obj_set_style_text_color(zero_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);

        // Backspace button
        lv_obj_t *backspace_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(backspace_btn, num_button_width, num_button_height);
        lv_obj_set_pos(backspace_btn, num_button_spacing + 2 * (num_button_width + num_button_spacing), bottom_row_y);
        lv_obj_add_event_cb(backspace_btn, keypadEventHandler, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t *backspace_label = lv_label_create(backspace_btn);
        lv_label_set_text(backspace_label, LV_SYMBOL_BACKSPACE);
        lv_obj_center(backspace_label);

        // Style for backspace button (Yellow)
        lv_obj_set_style_bg_color(backspace_btn, lv_color_hex(0xFFC107), LV_PART_MAIN);
        lv_obj_set_style_text_color(backspace_btn, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_color(backspace_btn, lv_color_hex(0xFFA000), LV_STATE_PRESSED);
        lv_obj_set_style_text_color(backspace_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);

        // Last row: Settings, Clear, and Go
        int last_row_y = bottom_row_y + num_button_height + num_button_spacing;

        // Settings button
        lv_obj_t *settings_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(settings_btn, num_button_width, num_button_height);
        lv_obj_set_pos(settings_btn, num_button_spacing, last_row_y);
        lv_obj_add_event_cb(settings_btn, navigationEventHandler, LV_EVENT_CLICKED, (void*)SCREEN_SETTINGS);

        lv_obj_t * settings_label = lv_label_create(settings_btn);
        lv_label_set_text(settings_label, LV_SYMBOL_SETTINGS);
        lv_obj_center(settings_label);
        
        // Style for settings button (Grey)
        lv_obj_set_style_bg_color(settings_btn, lv_color_hex(0x9E9E9E), LV_PART_MAIN);
        lv_obj_set_style_text_color(settings_btn, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_color(settings_btn, lv_color_hex(0x616161), LV_STATE_PRESSED);
        lv_obj_set_style_text_color(settings_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);

        // Clear button
        lv_obj_t *clear_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(clear_btn, num_button_width, num_button_height);
        lv_obj_set_pos(clear_btn, num_button_spacing + (num_button_width + num_button_spacing), last_row_y);
        lv_obj_add_event_cb(clear_btn, keypadEventHandler, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_bg_color(clear_btn, lv_color_hex(0xFF0000), LV_PART_MAIN);
        
        lv_obj_t *clear_label = lv_label_create(clear_btn);
        lv_label_set_text(clear_label, "Clear");
        lv_obj_set_style_text_font(clear_label, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(clear_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_center(clear_label);

        // Go button - Green
        lv_obj_t *go_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(go_btn, num_button_width, num_button_height);
        lv_obj_set_pos(go_btn, num_button_spacing + 2 * (num_button_width + num_button_spacing), last_row_y);
        lv_obj_add_event_cb(go_btn, keypadEventHandler, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_bg_color(go_btn, lv_color_hex(0x00FF00), LV_PART_MAIN);
        
        lv_obj_t *go_label = lv_label_create(go_btn);
        lv_label_set_text(go_label, "Go");
        lv_obj_set_style_text_font(go_label, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(go_label, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_center(go_label);
        
        // Set WiFi status label reference for WiFi module
        WiFiManager::setMainStatusLabel(main_wifi_status_label);
    }
    
    void createSettingsScreen() {
        // Create main container
        lv_obj_t* main_container = lv_obj_create(lv_scr_act());
        lv_obj_set_size(main_container, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(main_container, lv_color_hex(Colors::BACKGROUND), LV_PART_MAIN);
        lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(main_container, 10, LV_PART_MAIN);
        
        // Title
        lv_obj_t* title = lv_label_create(main_container);
        lv_label_set_text(title, "Settings");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);
        lv_obj_set_style_text_font(title, Fonts::FONT_LARGE, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(Colors::TEXT), 0);
        
        // WiFi status
        main_wifi_status_label = lv_label_create(main_container);
        lv_label_set_text(main_wifi_status_label, LV_SYMBOL_WIFI " Not Connected");
        lv_obj_align(main_wifi_status_label, LV_ALIGN_TOP_RIGHT, 0, 13);
        lv_obj_set_style_text_color(main_wifi_status_label, lv_color_hex(0x9E9E9E), 0);
        
        // Device Settings button
        lv_obj_t* shop_btn = lv_btn_create(main_container);
        lv_obj_set_size(shop_btn, lv_pct(100), 50);
        lv_obj_align(shop_btn, LV_ALIGN_TOP_MID, 0, 60);
        lv_obj_set_style_bg_color(shop_btn, lv_color_hex(Colors::PRIMARY), LV_PART_MAIN);
        lv_obj_add_event_cb(shop_btn, navigationEventHandler, LV_EVENT_CLICKED, (void*)SCREEN_SETTINGS_SUB);
        
        lv_obj_t* shop_label = lv_label_create(shop_btn);
        lv_label_set_text(shop_label, "Device Settings");
        lv_obj_center(shop_label);
        
        // WiFi settings button
        lv_obj_t* wifi_btn = lv_btn_create(main_container);
        lv_obj_set_size(wifi_btn, lv_pct(100), 50);
        lv_obj_align(wifi_btn, LV_ALIGN_TOP_MID, 0, 120);
        lv_obj_set_style_bg_color(wifi_btn, lv_color_hex(Colors::PRIMARY), LV_PART_MAIN);
        lv_obj_add_event_cb(wifi_btn, navigationEventHandler, LV_EVENT_CLICKED, (void*)SCREEN_WIFI);
        
        lv_obj_t* wifi_label = lv_label_create(wifi_btn);
        lv_label_set_text(wifi_label, "WiFi Settings");
        lv_obj_center(wifi_label);
        
        // Device info button
        lv_obj_t* info_btn = lv_btn_create(main_container);
        lv_obj_set_size(info_btn, lv_pct(100), 50);
        lv_obj_align(info_btn, LV_ALIGN_TOP_MID, 0, 180);
        lv_obj_set_style_bg_color(info_btn, lv_color_hex(Colors::INFO), LV_PART_MAIN);
        lv_obj_add_event_cb(info_btn, navigationEventHandler, LV_EVENT_CLICKED, (void*)SCREEN_INFO);
        
        lv_obj_t* info_label = lv_label_create(info_btn);
        lv_label_set_text(info_label, "Device Information");
        lv_obj_center(info_label);
        
        // Reboot Device button
        lv_obj_t* reboot_btn = lv_btn_create(main_container);
        lv_obj_set_size(reboot_btn, lv_pct(100), 50);
        lv_obj_align(reboot_btn, LV_ALIGN_TOP_MID, 0, 240);
        lv_obj_set_style_bg_color(reboot_btn, lv_color_hex(0xFF5722), LV_PART_MAIN); // Orange/Red color for reboot
        lv_obj_add_event_cb(reboot_btn, rebootDeviceEventHandler, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t* reboot_label = lv_label_create(reboot_btn);
        lv_label_set_text(reboot_label, LV_SYMBOL_REFRESH " Reboot Device");
        lv_obj_center(reboot_label);
        
        // AP Mode section - moved down by 60 pixels to accommodate reboot button
        if (WiFiManager::isAPModeActive()) {
            // Exit AP Mode button
            lv_obj_t* exit_ap_btn = lv_btn_create(main_container);
            lv_obj_set_size(exit_ap_btn, lv_pct(100), 50);
            lv_obj_align(exit_ap_btn, LV_ALIGN_TOP_MID, 0, 300);
            lv_obj_set_style_bg_color(exit_ap_btn, lv_color_hex(Colors::WARNING), LV_PART_MAIN);
            lv_obj_add_event_cb(exit_ap_btn, WiFiManager::exitAPModeEventHandler, LV_EVENT_CLICKED, NULL);
            
            lv_obj_t* exit_ap_label = lv_label_create(exit_ap_btn);
            lv_label_set_text(exit_ap_label, "Exit NWC Pairing Code Settings");
            lv_obj_center(exit_ap_label);
            
            // AP Status info
            lv_obj_t* ap_info = lv_label_create(main_container);
            String ap_text = "NWC Pairing Code AP Active\nSSID: " + WiFiManager::getAPSSID() + 
                           "\nPassword: " + WiFiManager::getAPPassword() + 
                           "\nIP: " + WiFiManager::getAPIP();
            lv_label_set_text(ap_info, ap_text.c_str());
            lv_obj_align(ap_info, LV_ALIGN_TOP_MID, 0, 360);
            lv_obj_set_style_text_color(ap_info, lv_color_hex(Colors::SUCCESS), 0);
            lv_label_set_long_mode(ap_info, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(ap_info, lv_pct(100));
        } else {
            // Launch AP Mode button
            lv_obj_t* launch_ap_btn = lv_btn_create(main_container);
            lv_obj_set_size(launch_ap_btn, lv_pct(100), 50);
            lv_obj_align(launch_ap_btn, LV_ALIGN_TOP_MID, 0, 300);
            lv_obj_set_style_bg_color(launch_ap_btn, lv_color_hex(Colors::SUCCESS), LV_PART_MAIN);
            lv_obj_add_event_cb(launch_ap_btn, WiFiManager::launchAPModeEventHandler, LV_EVENT_CLICKED, NULL);
            
            lv_obj_t* launch_ap_label = lv_label_create(launch_ap_btn);
            lv_label_set_text(launch_ap_label, "Set NWC Pairing Code");
            lv_obj_center(launch_ap_label);
        }
        
        // Back button - old style at top left
        lv_obj_t* back_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(back_btn, 40, 40);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
        lv_obj_add_event_cb(back_btn, navigationEventHandler, LV_EVENT_CLICKED, (void*)SCREEN_KEYPAD);
        
        lv_obj_t* back_label = lv_label_create(back_btn);
        lv_label_set_text(back_label, LV_SYMBOL_LEFT);
        lv_obj_center(back_label);
        
        // Style for Back button (transparent with white border)
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(back_btn, LV_OPA_0, LV_PART_MAIN);
        lv_obj_set_style_border_color(back_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_width(back_btn, 2, LV_PART_MAIN);
        lv_obj_set_style_text_color(back_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_radius(back_btn, 5, LV_PART_MAIN);
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x424242), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_text_color(back_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
        
        // Set WiFi status label reference
        WiFiManager::setMainStatusLabel(main_wifi_status_label);
    }
    
    void createWiFiScreen() {
        // Create main container
        lv_obj_t* main_container = lv_obj_create(lv_scr_act());
        lv_obj_set_size(main_container, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(main_container, lv_color_hex(Colors::BACKGROUND), LV_PART_MAIN);
        lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(main_container, 10, LV_PART_MAIN);
        
        // Title
        lv_obj_t* title = lv_label_create(main_container);
        lv_label_set_text(title, "WiFi Networks");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);
        lv_obj_set_style_text_font(title, Fonts::FONT_LARGE, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(Colors::TEXT), 0);
        
        // WiFi list
        wifi_list = lv_list_create(main_container);
        lv_obj_set_size(wifi_list, lv_pct(100), 350);
        lv_obj_align(wifi_list, LV_ALIGN_TOP_MID, 10, 50);
        lv_obj_set_style_bg_color(wifi_list, lv_color_hex(0x2c2c2c), LV_PART_MAIN);
        lv_obj_set_style_border_color(wifi_list, lv_color_hex(0x555555), LV_PART_MAIN);
        
        lv_list_add_text(wifi_list, "Press Scan to find networks");
        
        // Scan button - positioned beneath the WiFi list
        lv_obj_t* scan_btn = lv_btn_create(main_container);
        lv_obj_set_size(scan_btn, lv_pct(100), 40);
        lv_obj_align(scan_btn, LV_ALIGN_TOP_MID, 0, 400);
        lv_obj_set_style_bg_color(scan_btn, lv_color_hex(Colors::PRIMARY), LV_PART_MAIN);
        lv_obj_add_event_cb(scan_btn, WiFiManager::scanEventHandler, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t* scan_label = lv_label_create(scan_btn);
        lv_label_set_text(scan_label, "Scan");
        lv_obj_center(scan_label);
        
        // Back button - old style at top left
        lv_obj_t* back_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(back_btn, 40, 40);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
        lv_obj_add_event_cb(back_btn, navigationEventHandler, LV_EVENT_CLICKED, (void*)SCREEN_SETTINGS);
        
        lv_obj_t* back_label = lv_label_create(back_btn);
        lv_label_set_text(back_label, LV_SYMBOL_LEFT);
        lv_obj_center(back_label);
        
        // Style for Back button (transparent with white border)
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(back_btn, LV_OPA_0, LV_PART_MAIN);
        lv_obj_set_style_border_color(back_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_width(back_btn, 2, LV_PART_MAIN);
        lv_obj_set_style_text_color(back_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_radius(back_btn, 5, LV_PART_MAIN);
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x424242), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_text_color(back_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
        
        // Auto-start scan
        WiFiManager::scanEventHandler(NULL);
    }
    
    void createWiFiPasswordScreen(const char* ssid) {
        current_screen = SCREEN_WIFI_PASSWORD;
        
        lv_obj_clean(lv_scr_act());
        cleanupGlobalPointers();
        
        // Store the SSID for WiFi connection
        WiFiManager::setCurrentCredentials(ssid, "");
        
        // Create main container
        lv_obj_t* main_container = lv_obj_create(lv_scr_act());
        lv_obj_set_size(main_container, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(main_container, lv_color_hex(Colors::BACKGROUND), LV_PART_MAIN);
        lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(main_container, 10, LV_PART_MAIN);
        
        // Title
        lv_obj_t* title = lv_label_create(main_container);
        lv_label_set_text_fmt(title, "Connect to: %s", ssid);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_text_font(title, Fonts::FONT_DEFAULT, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(Colors::TEXT), 0);
        
        // Password input
        lv_obj_t* password_textarea = lv_textarea_create(main_container);
        lv_obj_set_size(password_textarea, lv_pct(100), 50);
        lv_obj_align(password_textarea, LV_ALIGN_TOP_MID, 0, 40);
        lv_textarea_set_placeholder_text(password_textarea, "Enter WiFi password");
        lv_textarea_set_password_mode(password_textarea, false);
        lv_textarea_set_one_line(password_textarea, true);
        
        // Status label (hidden initially)
        lv_obj_t* status_label = lv_label_create(main_container);
        lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
        WiFiManager::setStatusLabel(status_label);
        
        // Keyboard - full screen width
        lv_obj_t* kb = lv_keyboard_create(lv_scr_act());
        lv_keyboard_set_textarea(kb, password_textarea);
        lv_obj_add_event_cb(kb, WiFiManager::passwordKBEventHandler, LV_EVENT_ALL, NULL);
        
        // Back button - old style at top left
        lv_obj_t* back_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(back_btn, 40, 40);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
        lv_obj_add_event_cb(back_btn, WiFiManager::passwordBackEventHandler, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t* back_label = lv_label_create(back_btn);
        lv_label_set_text(back_label, LV_SYMBOL_LEFT);
        lv_obj_center(back_label);
        
        // Style for Back button (transparent with white border)
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(back_btn, LV_OPA_0, LV_PART_MAIN);
        lv_obj_set_style_border_color(back_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_width(back_btn, 2, LV_PART_MAIN);
        lv_obj_set_style_text_color(back_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_radius(back_btn, 5, LV_PART_MAIN);
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x424242), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_text_color(back_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
    }
    
    void createSettingsSubScreen() {
        // Create main container
        lv_obj_t* main_container = lv_obj_create(lv_scr_act());
        lv_obj_set_size(main_container, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(main_container, lv_color_hex(Colors::BACKGROUND), LV_PART_MAIN);
        lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(main_container, 10, LV_PART_MAIN);
                
        // Back button
        lv_obj_t* back_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(back_btn, 40, 40);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
        lv_obj_add_event_cb(back_btn, settingsBackEventHandler, LV_EVENT_CLICKED, NULL);
        
        // Ensure back button is on top
        lv_obj_move_foreground(back_btn);
        
        lv_obj_t* back_label = lv_label_create(back_btn);
        lv_label_set_text(back_label, LV_SYMBOL_LEFT);
        lv_obj_center(back_label);
        
        // Style for Back button (transparent with white border)
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(back_btn, LV_OPA_0, LV_PART_MAIN);
        lv_obj_set_style_border_color(back_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_width(back_btn, 2, LV_PART_MAIN);
        lv_obj_set_style_text_color(back_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_radius(back_btn, 5, LV_PART_MAIN);
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x424242), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_text_color(back_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
        
        // Title
        lv_obj_t* title = lv_label_create(main_container);
        lv_label_set_text(title, "Device Settings");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);
        lv_obj_set_style_text_font(title, Fonts::FONT_LARGE, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(Colors::TEXT), 0);
        
        // Currency selection
        lv_obj_t* currency_label = lv_label_create(main_container);
        lv_label_set_text(currency_label, "Currency:");
        lv_obj_align(currency_label, LV_ALIGN_TOP_LEFT, 0, 50);
        
        lv_obj_t* currency_dropdown = lv_dropdown_create(main_container);
        lv_dropdown_set_options(currency_dropdown, "sats\nUSD\nGBP\nEUR\nCHF");
        
        // Set current currency selection
        if (Settings::getCurrency() == "sats") lv_dropdown_set_selected(currency_dropdown, 0);
        else if (Settings::getCurrency() == "USD") lv_dropdown_set_selected(currency_dropdown, 1);
        else if (Settings::getCurrency() == "GBP") lv_dropdown_set_selected(currency_dropdown, 2);
        else if (Settings::getCurrency() == "EUR") lv_dropdown_set_selected(currency_dropdown, 3);
        else if (Settings::getCurrency() == "CHF") lv_dropdown_set_selected(currency_dropdown, 4);
        
        lv_obj_set_size(currency_dropdown, 100, 40);
        lv_obj_align(currency_dropdown, LV_ALIGN_TOP_LEFT, 120, 40);
        lv_obj_add_event_cb(currency_dropdown, currencyDropdownEventHandler, LV_EVENT_VALUE_CHANGED, NULL);
        
        // Shop name
        lv_obj_t* shop_name_label = lv_label_create(main_container);
        lv_label_set_text(shop_name_label, "Shop Name:");
        lv_obj_align(shop_name_label, LV_ALIGN_TOP_LEFT, 0, 110);
        
        shop_name_textarea = lv_textarea_create(main_container);
        lv_obj_set_size(shop_name_textarea, 180, 40);
        lv_obj_align(shop_name_textarea, LV_ALIGN_TOP_LEFT, 120, 100);
        lv_textarea_set_text(shop_name_textarea, Settings::getShopName().c_str());
        lv_textarea_set_one_line(shop_name_textarea, true);
        lv_obj_add_event_cb(shop_name_textarea, [](lv_event_t *e) {
            lv_event_code_t code = lv_event_get_code(e);
            if (code == LV_EVENT_CLICKED) {
                // Reset activity timer on text area click
                App::resetActivityTimer();
                if (shop_name_keyboard != NULL) {
                    lv_keyboard_set_textarea(shop_name_keyboard, shop_name_textarea);
                    lv_obj_clear_flag(shop_name_keyboard, LV_OBJ_FLAG_HIDDEN);
                    
                    // Hide PIN and Save buttons when keyboard is shown
                    if (settings_pin_btn != NULL) lv_obj_add_flag(settings_pin_btn, LV_OBJ_FLAG_HIDDEN);
                    if (settings_save_btn != NULL) lv_obj_add_flag(settings_save_btn, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }, LV_EVENT_CLICKED, NULL);
        
        // AP Password
        lv_obj_t* ap_password_label = lv_label_create(main_container);
        lv_label_set_text(ap_password_label, "AP Password:");
        lv_obj_align(ap_password_label, LV_ALIGN_TOP_LEFT, 0, 170);
        
        ap_password_textarea = lv_textarea_create(main_container);
        lv_obj_set_size(ap_password_textarea, 180, 40);
        lv_obj_align(ap_password_textarea, LV_ALIGN_TOP_LEFT, 120, 160);
        lv_textarea_set_text(ap_password_textarea, Settings::getAPPassword().c_str());
        lv_textarea_set_one_line(ap_password_textarea, true);
        lv_obj_add_event_cb(ap_password_textarea, [](lv_event_t *e) {
            lv_event_code_t code = lv_event_get_code(e);
            if (code == LV_EVENT_CLICKED) {
                // Reset activity timer on text area click
                App::resetActivityTimer();
                if (ap_password_keyboard != NULL) {
                    lv_keyboard_set_textarea(ap_password_keyboard, ap_password_textarea);
                    lv_obj_clear_flag(ap_password_keyboard, LV_OBJ_FLAG_HIDDEN);
                    
                    // Hide PIN and Save buttons when keyboard is shown
                    if (settings_pin_btn != NULL) lv_obj_add_flag(settings_pin_btn, LV_OBJ_FLAG_HIDDEN);
                    if (settings_save_btn != NULL) lv_obj_add_flag(settings_save_btn, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }, LV_EVENT_CLICKED, NULL);
        
        // PIN Management button
        settings_pin_btn = lv_btn_create(main_container);
        lv_obj_set_size(settings_pin_btn, 120, 40);
        lv_obj_align(settings_pin_btn, LV_ALIGN_TOP_LEFT, 0, 220);
        lv_obj_set_style_bg_color(settings_pin_btn, lv_color_hex(Colors::WARNING), LV_PART_MAIN);
        lv_obj_add_event_cb(settings_pin_btn, [](lv_event_t *e) {
            lv_event_code_t code = lv_event_get_code(e);
            if (code == LV_EVENT_CLICKED) {
                // Reset activity timer on PIN button click
                App::resetActivityTimer();
                Settings::showPinManagementScreen();
            }
        }, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t* pin_label = lv_label_create(settings_pin_btn);
        lv_label_set_text(pin_label, "Change PIN");
        lv_obj_center(pin_label);
        
        // Save button
        settings_save_btn = lv_btn_create(main_container);
        lv_obj_set_size(settings_save_btn, 100, 40);
        lv_obj_align(settings_save_btn, LV_ALIGN_TOP_RIGHT, 0, 220);
        lv_obj_set_style_bg_color(settings_save_btn, lv_color_hex(Colors::SUCCESS), LV_PART_MAIN);
        lv_obj_add_event_cb(settings_save_btn, settingsSaveEventHandler, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t* save_label = lv_label_create(settings_save_btn);
        lv_label_set_text(save_label, "Save");
        lv_obj_center(save_label);
        
        // Create keyboards (hidden initially)
        shop_name_keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_add_flag(shop_name_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(shop_name_keyboard, shopNameKBEventHandler, LV_EVENT_ALL, NULL);
        
        ap_password_keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_add_flag(ap_password_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(ap_password_keyboard, apPasswordKBEventHandler, LV_EVENT_ALL, NULL);
        
        // Set UI element references for Settings module
        Settings::setSettingsUIElements(settings_pin_btn, settings_save_btn);
        Settings::setShopNameTextArea(shop_name_textarea);
        Settings::setAPPasswordTextArea(ap_password_textarea);
    }
    
    void createInfoScreen() {
        // Create main container
        lv_obj_t* main_container = lv_obj_create(lv_scr_act());
        lv_obj_set_size(main_container, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(main_container, lv_color_hex(Colors::BACKGROUND), LV_PART_MAIN);
        lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(main_container, 10, LV_PART_MAIN);
        
        // Title
        lv_obj_t* title = lv_label_create(main_container);
        lv_label_set_text(title, "Device Information");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);
        lv_obj_set_style_text_font(title, Fonts::FONT_LARGE, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(Colors::TEXT), 0);
        
        // Nostr Relay info
        lv_obj_t* relay_info_label = lv_label_create(main_container);
        lv_label_set_text(relay_info_label, "Nostr Relay:");
        lv_obj_align(relay_info_label, LV_ALIGN_TOP_LEFT, 0, 60);
        lv_obj_set_style_text_font(relay_info_label, Fonts::FONT_DEFAULT, LV_PART_MAIN);
        lv_obj_set_style_text_color(relay_info_label, lv_color_hex(Colors::TEXT), 0);
        
        lv_obj_t* relay_url_label = lv_label_create(main_container);
        lv_label_set_text(relay_url_label, NWC::getRelayUrl().c_str());
        lv_obj_align(relay_url_label, LV_ALIGN_TOP_LEFT, 0, 80);
        lv_obj_set_style_text_font(relay_url_label, Fonts::FONT_SMALL, LV_PART_MAIN);
        lv_obj_set_style_text_color(relay_url_label, lv_color_hex(Colors::TEXT), 0);
        lv_label_set_long_mode(relay_url_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(relay_url_label, lv_pct(90));
        
        // Wallet Service Public Key
        lv_obj_t* wallet_pubkey_label = lv_label_create(main_container);
        lv_label_set_text(wallet_pubkey_label, "Wallet Service Public Key:");
        lv_obj_align(wallet_pubkey_label, LV_ALIGN_TOP_LEFT, 0, 115);
        lv_obj_set_style_text_font(wallet_pubkey_label, Fonts::FONT_DEFAULT, LV_PART_MAIN);
        lv_obj_set_style_text_color(wallet_pubkey_label, lv_color_hex(Colors::TEXT), 0);
        
        lv_obj_t* wallet_pubkey_value = lv_label_create(main_container);
        lv_label_set_text(wallet_pubkey_value, NWC::getWalletPubKey().c_str());
        lv_obj_align(wallet_pubkey_value, LV_ALIGN_TOP_LEFT, 0, 135);
        lv_obj_set_style_text_font(wallet_pubkey_value, Fonts::FONT_SMALL, LV_PART_MAIN);
        lv_obj_set_style_text_color(wallet_pubkey_value, lv_color_hex(Colors::TEXT), 0);
        lv_label_set_long_mode(wallet_pubkey_value, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(wallet_pubkey_value, lv_pct(90));
        
        // Device Public Key
        lv_obj_t* device_pubkey_label = lv_label_create(main_container);
        lv_label_set_text(device_pubkey_label, "Device Public Key:");
        lv_obj_align(device_pubkey_label, LV_ALIGN_TOP_LEFT, 0, 170);
        lv_obj_set_style_text_font(device_pubkey_label, Fonts::FONT_DEFAULT, LV_PART_MAIN);
        lv_obj_set_style_text_color(device_pubkey_label, lv_color_hex(Colors::TEXT), 0);
        
        lv_obj_t* device_pubkey_value = lv_label_create(main_container);
        lv_label_set_text(device_pubkey_value, NWC::getNpubHex().c_str());
        lv_obj_align(device_pubkey_value, LV_ALIGN_TOP_LEFT, 0, 190);
        lv_obj_set_style_text_font(device_pubkey_value, Fonts::FONT_SMALL, LV_PART_MAIN);
        lv_obj_set_style_text_color(device_pubkey_value, lv_color_hex(Colors::TEXT), 0);
        lv_label_set_long_mode(device_pubkey_value, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(device_pubkey_value, lv_pct(90));
        
        // Software Version
        lv_obj_t* version_label = lv_label_create(main_container);
        lv_label_set_text(version_label, "Software Version:");
        lv_obj_align(version_label, LV_ALIGN_TOP_LEFT, 0, 225);
        lv_obj_set_style_text_font(version_label, Fonts::FONT_DEFAULT, LV_PART_MAIN);
        lv_obj_set_style_text_color(version_label, lv_color_hex(Colors::TEXT), 0);
        
        lv_obj_t* version_value = lv_label_create(main_container);
        lv_label_set_text(version_value, App::getVersion().c_str());
        lv_obj_align(version_value, LV_ALIGN_TOP_LEFT, 0, 245);
        lv_obj_set_style_text_font(version_value, Fonts::FONT_SMALL, LV_PART_MAIN);
        lv_obj_set_style_text_color(version_value, lv_color_hex(Colors::TEXT), 0);
        
        // Hardware Information
        lv_obj_t* hardware_label = lv_label_create(main_container);
        lv_label_set_text(hardware_label, "Hardware Information:");
        lv_obj_align(hardware_label, LV_ALIGN_TOP_LEFT, 0, 280);
        lv_obj_set_style_text_font(hardware_label, Fonts::FONT_DEFAULT, LV_PART_MAIN);
        lv_obj_set_style_text_color(hardware_label, lv_color_hex(Colors::TEXT), 0);
        
        lv_obj_t* hardware_info = lv_label_create(main_container);
        String hardware_text = "ESP32 - WT32-SC01\n";
        hardware_text += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
        hardware_text += "WiFi MAC: " + WiFi.macAddress() + "\n";
        
        if (WiFiManager::isConnected()) {
            hardware_text += "WiFi: Connected (" + WiFiManager::getSSID() + ")\n";
            hardware_text += "IP: " + WiFiManager::getLocalIP();
        } else {
            hardware_text += "WiFi: Not Connected";
        }
        
        lv_label_set_text(hardware_info, hardware_text.c_str());
        lv_obj_align(hardware_info, LV_ALIGN_TOP_LEFT, 0, 300);
        lv_obj_set_style_text_font(hardware_info, Fonts::FONT_SMALL, LV_PART_MAIN);
        lv_obj_set_style_text_color(hardware_info, lv_color_hex(Colors::TEXT), 0);
        lv_label_set_long_mode(hardware_info, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(hardware_info, lv_pct(90));
        
        // Back button
        lv_obj_t* back_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(back_btn, 40, 40);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
        lv_obj_add_event_cb(back_btn, navigationEventHandler, LV_EVENT_CLICKED, (void*)SCREEN_SETTINGS);
        
        lv_obj_t* back_label = lv_label_create(back_btn);
        lv_label_set_text(back_label, LV_SYMBOL_LEFT);
        lv_obj_center(back_label);
        
        // Style for Back button (transparent with white border)
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(back_btn, LV_OPA_0, LV_PART_MAIN);
        lv_obj_set_style_border_color(back_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_width(back_btn, 2, LV_PART_MAIN);
        lv_obj_set_style_text_color(back_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_radius(back_btn, 5, LV_PART_MAIN);
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x424242), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_text_color(back_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
    }
    
    void createInvoiceOverlay() {
        if (invoice_overlay != NULL) {
            return; // Already exists
        }
        
        invoice_overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(invoice_overlay, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(invoice_overlay, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(invoice_overlay, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(invoice_overlay, 0, LV_PART_MAIN);
        
        // Amount display above QR code
        invoice_amount_label = lv_label_create(invoice_overlay);
        lv_label_set_text(invoice_amount_label, "Generating invoice...");
        lv_obj_align(invoice_amount_label, LV_ALIGN_CENTER, 0, -180);
        lv_obj_set_style_text_color(invoice_amount_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(invoice_amount_label, Fonts::FONT_XLARGE, LV_PART_MAIN);
        lv_obj_set_style_text_align(invoice_amount_label, LV_TEXT_ALIGN_CENTER, 0);
        
        // QR Code canvas
        qr_canvas = lv_canvas_create(invoice_overlay);
        lv_obj_set_size(qr_canvas, 280, 280);
        lv_obj_align(qr_canvas, LV_ALIGN_CENTER, 0, 10);
        lv_obj_set_style_bg_color(qr_canvas, lv_color_white(), LV_PART_MAIN);
        
        // Invoice status label (below QR code)
        invoice_label = lv_label_create(invoice_overlay);
        lv_label_set_text(invoice_label, "Scan QR code to pay");
        lv_obj_align(invoice_label, LV_ALIGN_CENTER, 0, 150);
        lv_obj_set_style_text_color(invoice_label, lv_color_black(), 0);
        lv_obj_set_style_text_align(invoice_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(invoice_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(invoice_label, 260);
        
        // Spinner
        invoice_spinner = lv_spinner_create(invoice_overlay, 1000, 60);
        lv_obj_set_size(invoice_spinner, 40, 40);
        lv_obj_align(invoice_spinner, LV_ALIGN_CENTER, 0, 0);
        
        // Close button
        lv_obj_t* close_btn = lv_btn_create(invoice_overlay);
        lv_obj_set_size(close_btn, 100, 40);
        lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(close_btn, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_border_color(close_btn, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_border_width(close_btn, 2, LV_PART_MAIN);
        lv_obj_add_event_cb(close_btn, invoiceCloseButtonEventHandler, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t* close_label = lv_label_create(close_btn);
        lv_label_set_text(close_label, "Close");
        lv_obj_set_style_text_color(close_label, lv_color_black(), 0);
        lv_obj_center(close_label);
        
        invoice_processing = true;
        
        // Set references for Display module
        Display::setQRCanvas(qr_canvas);
    }
    
    void closeInvoiceOverlay() {
        if (invoice_overlay != NULL) {
            lv_obj_del(invoice_overlay);
            invoice_overlay = NULL;
            qr_canvas = NULL;
            invoice_label = NULL;
            invoice_spinner = NULL;
            invoice_amount_label = NULL;
        }
        invoice_processing = false;
        
        // Stop NWC timers when overlay is closed
        NWC::stopInvoiceLookupTimer();
        NWC::stopInvoiceNotificationWatchdog();
    }
    
    void updateInvoiceDisplay(const String& invoice, int amount_sats) {
        Serial.println("=== Starting updateInvoiceDisplay ===");
        Serial.println("Invoice length: " + String(invoice.length()));
        Serial.println("Amount: " + String(amount_sats) + " sats");
        Serial.println("Free heap before QR: " + String(ESP.getFreeHeap()));
        
        // Remove spinner if it exists
        if (invoice_spinner != NULL && lv_obj_is_valid(invoice_spinner)) {
            lv_obj_del(invoice_spinner);
            invoice_spinner = NULL;
        }
        
        // Update the amount label with sats and fiat value
        if (invoice_amount_label != NULL && lv_obj_is_valid(invoice_amount_label)) {
            // Calculate fiat value from sats (reverse of calculateSatsFromAmount)
            String currency = Settings::getCurrency();
            String amount_text = "Please pay\n" + String(amount_sats) + " sats";
            
            // Add fiat conversion if not in sats currency and Bitcoin prices are available
            if (currency != "sats" && NWC::arePricesLoaded()) {
                long* btc_prices = NWC::getBitcoinPrices();
                long btc_price = 0;
                
                // Get price based on currency
                if (currency == "USD") btc_price = btc_prices[0];
                else if (currency == "EUR") btc_price = btc_prices[1];
                else if (currency == "GBP") btc_price = btc_prices[2];
                else if (currency == "CAD") btc_price = btc_prices[3];
                else if (currency == "CHF") btc_price = btc_prices[4];
                else if (currency == "AUD") btc_price = btc_prices[5];
                else if (currency == "JPY") btc_price = btc_prices[6];
                
                if (btc_price > 0) {
                    // Convert sats to fiat: (sats / 100,000,000) * btc_price
                    float fiat_amount = (float)amount_sats / 100000000.0 * btc_price;
                    amount_text += "\n(" + String(fiat_amount, 2) + " " + currency + ")";
                }
            }
            
            lv_label_set_text(invoice_amount_label, amount_text.c_str());
            lv_obj_clear_flag(invoice_amount_label, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Update the status label
        if (invoice_label != NULL && lv_obj_is_valid(invoice_label)) {
            lv_label_set_text(invoice_label, "Scan QR code to pay");
            lv_obj_clear_flag(invoice_label, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Display QR code using Display module to avoid duplication
        Display::displayQRCode(invoice);
        
        Serial.println("Free heap after QR: " + String(ESP.getFreeHeap()));
        Serial.println("=== updateInvoiceDisplay completed ===");
    }
    
    void showPaymentReceived() {
        Serial.println("=== showPaymentReceived called ===");
        
        if (invoice_overlay == NULL || !lv_obj_is_valid(invoice_overlay)) {
            Serial.println("No invoice overlay to update");
            return;
        }
        
        // Hide QR code
        if (qr_canvas != NULL && lv_obj_is_valid(qr_canvas)) {
            lv_obj_add_flag(qr_canvas, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Create checkmark symbol
        lv_obj_t* checkmark = lv_label_create(invoice_overlay);
        lv_label_set_text(checkmark, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(checkmark, lv_color_hex(Colors::SUCCESS), 0);
        lv_obj_set_style_text_font(checkmark, &lv_font_montserrat_48, 0);
        lv_obj_align(checkmark, LV_ALIGN_CENTER, 0, -40);
        
        // Update status label
        if (invoice_label != NULL && lv_obj_is_valid(invoice_label)) {
            lv_label_set_text(invoice_label, "Payment received!");
            lv_obj_set_style_text_color(invoice_label, lv_color_hex(Colors::SUCCESS), 0);
            lv_obj_clear_flag(invoice_label, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Auto-close overlay after 3 seconds
        lv_timer_create([](lv_timer_t* timer) {
            closeInvoiceOverlay();
            lv_timer_del(timer);
        }, 3000, NULL);
        
        Serial.println("=== showPaymentReceived completed ===");
    }
    
    void showMessage(String title, String message) {
        // Create message overlay
        lv_obj_t* msg_overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(msg_overlay, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(msg_overlay, lv_color_hex(Colors::BACKGROUND), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(msg_overlay, LV_OPA_80, LV_PART_MAIN);
        lv_obj_set_style_border_width(msg_overlay, 0, LV_PART_MAIN);
        
        // Message box
        lv_obj_t* msg_box = lv_obj_create(msg_overlay);
        lv_obj_set_size(msg_box, 280, 280);
        lv_obj_center(msg_box);
        lv_obj_set_style_bg_color(msg_box, lv_color_hex(0x2c2c2c), LV_PART_MAIN);
        lv_obj_set_style_border_color(msg_box, lv_color_hex(Colors::PRIMARY), LV_PART_MAIN);
        lv_obj_set_style_border_width(msg_box, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(msg_box, 10, LV_PART_MAIN);
        
        // Title
        lv_obj_t* title_label = lv_label_create(msg_box);
        lv_label_set_text(title_label, title.c_str());
        lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);
        lv_obj_set_style_text_font(title_label, Fonts::FONT_DEFAULT, LV_PART_MAIN);
        lv_obj_set_style_text_color(title_label, lv_color_hex(Colors::TEXT), 0);
        
        // Message
        lv_obj_t* msg_label = lv_label_create(msg_box);
        lv_label_set_text(msg_label, message.c_str());
        lv_obj_align(msg_label, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_color(msg_label, lv_color_hex(Colors::TEXT), 0);
        lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(msg_label, 240);
        
        // OK button
        lv_obj_t* ok_btn = lv_btn_create(msg_box);
        lv_obj_set_size(ok_btn, 80, 35);
        lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_MID, 0, -15);
        lv_obj_set_style_bg_color(ok_btn, lv_color_hex(Colors::PRIMARY), LV_PART_MAIN);
        lv_obj_add_event_cb(ok_btn, [](lv_event_t *e) {
            // Reset activity timer on OK button click
            App::resetActivityTimer();
            lv_obj_t* overlay = (lv_obj_t*)lv_event_get_user_data(e);
            lv_obj_del(overlay);
        }, LV_EVENT_CLICKED, msg_overlay);
        
        lv_obj_t* ok_label = lv_label_create(ok_btn);
        lv_label_set_text(ok_label, "OK");
        lv_obj_center(ok_label);
        
        Serial.println(title + ": " + message);
    }
    
    // Event handlers
    void keypadEventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t *btn = lv_event_get_target(e);
        
        if (code == LV_EVENT_CLICKED) {
            // Reset activity timer on any button press
            App::resetActivityTimer();
            
            /*Get the first child of the button which is the label and get its text*/
            lv_obj_t *label = lv_obj_get_child(btn, 0);
            const char *button_text = lv_label_get_text(label);
            
            if (display_label == NULL) return;
            
            static String entered_number = "";
            
            if (strcmp(button_text, LV_SYMBOL_BACKSPACE) == 0) {
                // Backspace button
                if (entered_number.length() > 0) {
                    entered_number.remove(entered_number.length() - 1);
                }
                Serial.println("Button pressed: Backspace");
            }
            else if (strcmp(button_text, "Clear") == 0) {
                // Clear button
                entered_number = "";
                Serial.println("Button pressed: Clear");
            }
            else if (strcmp(button_text, "Go") == 0) {
                // Go button
                Serial.print("Button pressed: Go - Number entered: ");
                Serial.println(entered_number);
                createInvoiceOverlay();
                // Request invoice creation through NWC module
                if (entered_number.length() > 0) {
                    float amount = entered_number.toFloat();
                    NWC::makeInvoice(amount, Settings::getCurrency());
                }
            }
            else if (strcmp(button_text, ".") == 0) {
                // Decimal button
                if (entered_number.indexOf('.') == -1) { // Only add decimal if not already present
                    entered_number += ".";
                }
                Serial.println("Button pressed: Decimal");
            }
            else {
                // Number button (0-9)
                int dot_index = entered_number.indexOf('.');
                if (dot_index == -1 || (entered_number.length() - dot_index - 1) < 2) {
                    entered_number += button_text;
                    Serial.print("Button pressed: ");
                    Serial.println(button_text);
                }
            }
            
            // Update display
            if (entered_number.isEmpty()) {
                lv_label_set_text(display_label, ("0 " + Settings::getCurrency()).c_str());
            } else {
                String display_text = entered_number + " " + Settings::getCurrency();
                lv_label_set_text(display_label, display_text.c_str());
            }
        }
    }
    
    void navigationEventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED) {
            // Reset activity timer on navigation
            App::resetActivityTimer();
            screen_state_t target_screen = (screen_state_t)(uintptr_t)lv_event_get_user_data(e);
            loadScreen(target_screen);
        }
    }
    
    void settingsSaveEventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED) {
            // Reset activity timer on settings save
            App::resetActivityTimer();
            // Get the current shop name from the text area
            if (shop_name_textarea != NULL) {
                const char* text = lv_textarea_get_text(shop_name_textarea);
                Settings::setShopName(String(text));
                Serial.println("Shop name updated from text area: " + Settings::getShopName());
            }
            
            // Get AP password from text area
            if (ap_password_textarea != NULL) {
                const char* text = lv_textarea_get_text(ap_password_textarea);
                Settings::setAPPassword(String(text));
                Serial.println("AP password updated from text area: " + Settings::getAPPassword());
            }
            
            Settings::saveToPreferences();
            showMessage("Settings Saved", "Shop settings have been saved successfully.");
            
            // Return to main settings screen
            loadScreen(SCREEN_SETTINGS);
        }
    }
    
    void settingsBackEventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED) {
            loadScreen(SCREEN_SETTINGS);
        }
    }
    
    void currencyDropdownEventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_VALUE_CHANGED) {
            lv_obj_t* dropdown = lv_event_get_target(e);
            uint16_t selected = lv_dropdown_get_selected(dropdown);
            
            switch (selected) {
                case 0:
                    Settings::setCurrency("sats");
                    break;
                case 1:
                    Settings::setCurrency("USD");
                    break;
                case 2:
                    Settings::setCurrency("GBP");
                    break;
                case 3:
                    Settings::setCurrency("EUR");
                    break;
                case 4:
                    Settings::setCurrency("CHF");
                    break;
            }
            
            Serial.println("Currency changed to: " + Settings::getCurrency());
        }
    }
    
    void shopNameKBEventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* kb = lv_event_get_target(e);
        
        if (code == LV_EVENT_READY) {
            // Reset activity timer on keyboard input
            App::resetActivityTimer();
            lv_obj_t* ta = lv_keyboard_get_textarea(kb);
            const char* text = lv_textarea_get_text(ta);
            Settings::setShopName(String(text));
            
            Serial.println("Shop name changed to: " + Settings::getShopName());
            
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            
            // Show the PIN and Save buttons when keyboard is hidden
            if (settings_pin_btn != NULL && lv_obj_is_valid(settings_pin_btn)) {
                lv_obj_clear_flag(settings_pin_btn, LV_OBJ_FLAG_HIDDEN);
            }
            if (settings_save_btn != NULL && lv_obj_is_valid(settings_save_btn)) {
                lv_obj_clear_flag(settings_save_btn, LV_OBJ_FLAG_HIDDEN);
            }
        } else if (code == LV_EVENT_CANCEL) {
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            
            // Show the PIN and Save buttons when keyboard is hidden
            if (settings_pin_btn != NULL && lv_obj_is_valid(settings_pin_btn)) {
                lv_obj_clear_flag(settings_pin_btn, LV_OBJ_FLAG_HIDDEN);
            }
            if (settings_save_btn != NULL && lv_obj_is_valid(settings_save_btn)) {
                lv_obj_clear_flag(settings_save_btn, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    
    void apPasswordKBEventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* kb = lv_event_get_target(e);
        
        if (code == LV_EVENT_READY) {
            // Reset activity timer on keyboard input
            App::resetActivityTimer();
            lv_obj_t* ta = lv_keyboard_get_textarea(kb);
            const char* text = lv_textarea_get_text(ta);
            Settings::setAPPassword(String(text));
            
            Serial.println("AP password changed to: " + Settings::getAPPassword());
            
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            
            // Show the PIN and Save buttons when keyboard is hidden
            if (settings_pin_btn != NULL && lv_obj_is_valid(settings_pin_btn)) {
                lv_obj_clear_flag(settings_pin_btn, LV_OBJ_FLAG_HIDDEN);
            }
            if (settings_save_btn != NULL && lv_obj_is_valid(settings_save_btn)) {
                lv_obj_clear_flag(settings_save_btn, LV_OBJ_FLAG_HIDDEN);
            }
        } else if (code == LV_EVENT_CANCEL) {
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            
            // Show the PIN and Save buttons when keyboard is hidden
            if (settings_pin_btn != NULL && lv_obj_is_valid(settings_pin_btn)) {
                lv_obj_clear_flag(settings_pin_btn, LV_OBJ_FLAG_HIDDEN);
            }
            if (settings_save_btn != NULL && lv_obj_is_valid(settings_save_btn)) {
                lv_obj_clear_flag(settings_save_btn, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    
    void invoiceCloseButtonEventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED) {
            // Reset activity timer on invoice close
            App::resetActivityTimer();
            closeInvoiceOverlay();
        }
    }
    
    void rebootDeviceEventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED) {
            // Reset activity timer on reboot button click
            App::resetActivityTimer();
            
            // Show confirmation dialog
            showMessage("Reboot Device", "Are you sure you want to reboot the device? This will restart all services.");
            
            // Create a timer to reboot after showing the message
            lv_timer_create([](lv_timer_t* timer) {
                Serial.println("Rebooting device...");
                showMessage("Rebooting...", "Please wait while the device reboots...");
                
                // Clean up any resources before reboot
                Display::turnOffBacklight();
                Display::cleanup();
                UI::cleanup();
                
                // Small delay to ensure cleanup is complete
                delay(500);
                
                // Reboot the ESP32
                ESP.restart();
                
                lv_timer_del(timer);
            }, 3000, NULL); // 3 second delay to show the message
        }
    }
    
    // Utility functions
    void updateAmountDisplay(const String& amount) {
        if (display_label != NULL) {
            String display_text = amount + " " + Settings::getCurrency();
            lv_label_set_text(display_label, display_text.c_str());
        }
    }
    
    void updateStatusDisplay(const String& status) {
        // Update any relevant status displays
        Serial.println("Status: " + status);
    }
    
    void showLoadingSpinner(bool show) {
        if (invoice_spinner != NULL) {
            if (show) {
                lv_obj_clear_flag(invoice_spinner, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(invoice_spinner, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    
    // Getters and setters
    lv_obj_t* getDisplayLabel() { return display_label; }
    lv_obj_t* getWiFiList() { return wifi_list; }
    lv_obj_t* getQRCanvas() { return qr_canvas; }
    lv_obj_t* getInvoiceLabel() { return invoice_label; }
    lv_obj_t* getInvoiceSpinner() { return invoice_spinner; }
    lv_obj_t* getMainWiFiStatusLabel() { return main_wifi_status_label; }
    
    void setDisplayLabel(lv_obj_t* label) { display_label = label; }
    void setWiFiList(lv_obj_t* list) { wifi_list = list; }
    void setQRCanvas(lv_obj_t* canvas) { qr_canvas = canvas; }
    void setInvoiceLabel(lv_obj_t* label) { invoice_label = label; }
    void setInvoiceSpinner(lv_obj_t* spinner) { invoice_spinner = spinner; }
    void setMainWiFiStatusLabel(lv_obj_t* label) { main_wifi_status_label = label; }
    
    screen_state_t getCurrentScreen() { return current_screen; }
    bool isOverlayActive() { return invoice_overlay != NULL; }
    bool isInvoiceProcessing() { return invoice_processing; }
    void setInvoiceProcessing(bool processing) { invoice_processing = processing; }
    
    void updateShopNameDisplay() {
        // Update shop name in settings screen if active
        if (shop_name_textarea != NULL) {
            lv_textarea_set_text(shop_name_textarea, Settings::getShopName().c_str());
        }
    }
    
    void updateCurrencyDisplay() {
        // Update display label with new currency
        if (display_label != NULL) {
            const char* current_text = lv_label_get_text(display_label);
            String amount_part = String(current_text);
            int space_pos = amount_part.lastIndexOf(' ');
            if (space_pos != -1) {
                amount_part = amount_part.substring(0, space_pos);
            }
            updateAmountDisplay(amount_part);
        }
    }
    
    void updateAPPasswordDisplay() {
        // Update AP password in settings screen if active
        if (ap_password_textarea != NULL) {
            lv_textarea_set_text(ap_password_textarea, Settings::getAPPassword().c_str());
        }
    }
}