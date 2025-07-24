#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace UI {
    // Screen state enumeration
    typedef enum {
        SCREEN_KEYPAD,
        SCREEN_SETTINGS,
        SCREEN_WIFI,
        SCREEN_WIFI_PASSWORD,
        SCREEN_SETTINGS_SUB,
        SCREEN_INFO
    } screen_state_t;

    // Initialization and cleanup
    void init();
    void cleanup();
    
    // Screen creation functions
    void createKeypadScreen();
    void createSettingsScreen();
    void createWiFiScreen();
    void createWiFiPasswordScreen(const char* ssid);
    void createSettingsSubScreen();
    void createInfoScreen();
    
    // Screen management
    void loadScreen(screen_state_t screen);
    void cleanupGlobalPointers();
    
    // Invoice overlay management
    void createInvoiceOverlay();
    void closeInvoiceOverlay();
    void updateInvoiceDisplay(const String& invoice, int amount_sats);
    void showPaymentReceived();
    bool isInvoiceProcessing();
    void setInvoiceProcessing(bool processing);
    
    // Message display
    void showMessage(String title, String message);
    
    // UI element accessors for other modules
    lv_obj_t* getDisplayLabel();
    lv_obj_t* getWiFiList();
    lv_obj_t* getQRCanvas();
    lv_obj_t* getInvoiceLabel();
    lv_obj_t* getInvoiceSpinner();
    lv_obj_t* getMainWiFiStatusLabel();
    
    // UI element setters
    void setDisplayLabel(lv_obj_t* label);
    void setWiFiList(lv_obj_t* list);
    void setQRCanvas(lv_obj_t* canvas);
    void setInvoiceLabel(lv_obj_t* label);
    void setInvoiceSpinner(lv_obj_t* spinner);
    void setMainWiFiStatusLabel(lv_obj_t* label);
    
    // Event handlers for external use
    void keypadEventHandler(lv_event_t* e);
    void navigationEventHandler(lv_event_t* e);
    void settingsSaveEventHandler(lv_event_t* e);
    void settingsBackEventHandler(lv_event_t* e);
    void currencyDropdownEventHandler(lv_event_t* e);
    void shopNameKBEventHandler(lv_event_t* e);
    void apPasswordKBEventHandler(lv_event_t* e);
    void invoiceCloseButtonEventHandler(lv_event_t* e);
    void rebootDeviceEventHandler(lv_event_t* e);
    
    // Utility functions
    void updateAmountDisplay(const String& amount);
    void updateStatusDisplay(const String& status);
    void showLoadingSpinner(bool show);
    
    // Settings integration
    void updateShopNameDisplay();
    void updateCurrencyDisplay();
    void updateAPPasswordDisplay();
    
    // Screen state queries
    screen_state_t getCurrentScreen();
    bool isOverlayActive();
    
    // Constants for UI styling
    namespace Colors {
        const uint32_t PRIMARY = 0x2196F3;
        const uint32_t SUCCESS = 0x4CAF50;
        const uint32_t WARNING = 0xFF9800;
        const uint32_t ERROR = 0xF44336;
        const uint32_t INFO = 0x607D8B;
        const uint32_t BACKGROUND = 0x000000;
        const uint32_t TEXT = 0xFFFFFF;
    }
    
    namespace Fonts {
        extern const lv_font_t* FONT_DEFAULT;
        extern const lv_font_t* FONT_LARGE;
        extern const lv_font_t* FONT_XLARGE;
        extern const lv_font_t* FONT_SMALL;
    }
}