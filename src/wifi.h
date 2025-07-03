#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <NTPClient.h>
#include <vector>
#include <lvgl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

namespace WiFiManager {
    // WiFi command types
    typedef enum {
        WIFI_SCAN,
        WIFI_CONNECT,
        WIFI_DISCONNECT,
        WIFI_STOP_SCAN
    } wifi_command_type_t;

    // WiFi command structure
    typedef struct {
        wifi_command_type_t type;
        char ssid[33];
        char password[65];
    } wifi_command_t;

    // WiFi scan result structure
    typedef struct {
        int network_count;
        char ssids[9][33];  // Store up to 9 SSIDs
        int rssi[9];
        bool encrypted[9];
    } wifi_scan_result_t;

    // Initialization
    void init();
    void cleanup();
    void processLoop();
    
    // Connection management
    void startConnection(const char* ssid, const char* password);
    void disconnect();
    bool isConnected();
    String getSSID();
    String getLocalIP();
    wl_status_t getStatus();
    
    // Network scanning
    void startScan();
    void stopScanning();
    bool processScanResults();
    
    // Credential management
    void saveCredentials(const char* ssid, const char* password);
    bool findSavedCredentials(const char* ssid, char* password, size_t password_size);
    void loadAllNetworks();
    void removeNetwork(const char* ssid);
    
    // Access Point mode
    void startAPMode();
    void stopAPMode();
    bool isAPModeActive();
    String getAPSSID();
    String getAPPassword();
    String getAPIP();
    
    // NWC URL management
    void loadNWCUrl();
    void saveNWCUrl(const String& url);
    String getNWCUrl();
    void setNWCUrl(const String& url);
    
    // Status monitoring
    void updateStatus();
    void setStatusLabel(lv_obj_t* label);
    void setMainStatusLabel(lv_obj_t* label);
    
    // Event handlers for UI integration
    void scanEventHandler(lv_event_t* e);
    void connectEventHandler(lv_event_t* e);
    void passwordKBEventHandler(lv_event_t* e);
    void passwordBackEventHandler(lv_event_t* e);
    void launchAPModeEventHandler(lv_event_t* e);
    void exitAPModeEventHandler(lv_event_t* e);
    
    // Task management
    void createTask();
    void deleteTask();
    TaskHandle_t getTaskHandle();
    
    // Queue management
    QueueHandle_t getCommandQueue();
    QueueHandle_t getScanResultQueue();
    
    // Utility functions
    void setCurrentCredentials(const char* ssid, const char* password);
    void getCurrentCredentials(char* ssid, char* password);
    std::vector<String>& getSSIDList();
    
    // Status callbacks for integration
    typedef void (*wifi_status_callback_t)(bool connected, const char* status);
    void setStatusCallback(wifi_status_callback_t callback);
    
    // NTP client access
    NTPClient& getNTPClient();
    
    // Timer management
    void createStatusTimer();
    void deleteStatusTimer();
    
    // AP mode web server handlers
    void handleAPRoot();
    void handleAPConfig();
    void updateSettingsScreenForAPMode();
}