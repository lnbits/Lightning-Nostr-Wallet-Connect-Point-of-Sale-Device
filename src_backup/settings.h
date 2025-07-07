#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>

namespace Settings {
    // Initialization and cleanup
    void init();
    void cleanup();
    
    // Shop settings
    String getCurrency();
    void setCurrency(const String& currency);
    
    String getShopName();
    void setShopName(const String& name);
    
    String getAPPassword();
    void setAPPassword(const String& password);
    
    // NWC configuration
    String getNWCUrl();
    void setNWCUrl(const String& url);
    
    // PIN management
    String getCurrentPin();
    void setCurrentPin(const String& pin);
    bool verifyPin(const String& pin);
    
    // WiFi network management
    void saveWiFiNetwork(const char* ssid, const char* password);
    void loadAllWiFiNetworks();
    
    // Persistence
    void loadFromPreferences();
    void saveToPreferences();
    
    // UI Event handlers
    void settingsSaveEventHandler(lv_event_t *e);
    void settingsBackEventHandler(lv_event_t *e);
    void shopNameKBEventHandler(lv_event_t *e);
    void apPasswordKBEventHandler(lv_event_t *e);
    
    // PIN management UI
    void showPinManagementScreen();
    void hidePinManagementScreen();
    void showPinVerificationScreen();
    
    // Configuration management
    void resetToDefaults();
    void pinCurrentKBEventHandler(lv_event_t *e);
    void pinNewKBEventHandler(lv_event_t *e);
    void pinVerifyKBEventHandler(lv_event_t *e);
    void pinSaveEventHandler(lv_event_t *e);
    void pinCancelEventHandler(lv_event_t *e);
    void pinVerificationKBEventHandler(lv_event_t *e);
    void pinVerificationCancelEventHandler(lv_event_t *e);
    
    // UI element references for keyboard handling
    void setSettingsUIElements(lv_obj_t *pin_btn, lv_obj_t *save_btn);
    void setShopNameTextArea(lv_obj_t *textarea);
    void setAPPasswordTextArea(lv_obj_t *textarea);
}