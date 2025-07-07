#include "wifi_manager.h"
#include <WiFi.h>
#include "settings.h"
#include "app.h"

#include "ui.h"
#include "nwc.h"

namespace WiFiManager {
    // Global WiFi state
    static WiFiUDP ntpUDP;
    static NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);
    
    // Connection management
    static unsigned long wifi_connect_start_time = 0;
    static const unsigned long WIFI_CONNECT_TIMEOUT = 10000; // 10 seconds
    static bool wifi_connection_attempted = false;
    static char current_ssid[33];
    static char current_password[65];
    
    // Access Point mode
    static bool ap_mode_active = false;
    static WebServer ap_server(80);
    static DNSServer dns_server;
    static const char* ap_ssid = "NWC-PoS-Portal";
    static const char* ap_ip = "192.168.4.1";
    
    // Task and queue management
    static TaskHandle_t wifi_task_handle = NULL;
    static QueueHandle_t wifi_command_queue = NULL;
    static QueueHandle_t wifi_scan_result_queue = NULL;
    
    // UI elements
    static lv_obj_t* wifi_status_label = NULL;
    static lv_obj_t* main_wifi_status_label = NULL;
    static lv_timer_t* wifi_scan_timer = NULL;
    static lv_timer_t* wifi_status_timer = NULL;
    
    // SSID storage for UI
    static std::vector<String> wifi_ssids;
    
    // Status callback
    static wifi_status_callback_t status_callback = nullptr;
    
    // Preferences instance
    static Preferences preferences;
    
    // WiFi task function - runs on Core 0
    static void wifiTask(void *parameter) {
        Serial.println("WiFi task started");
        while (true) {
            wifi_command_t command;
            if (xQueueReceive(wifi_command_queue, &command, portMAX_DELAY)) {
                Serial.print("WiFi task received command: ");
                Serial.println(command.type);
                
                switch (command.type) {
                    case WIFI_SCAN: {
                        Serial.println("Starting WiFi scan...");
                        
                        // Stop auto-reconnect and put WiFi in scan mode
                        WiFi.setAutoReconnect(false);
                        WiFi.disconnect(true);  // Disconnect and clear config
                        delay(1000);  // Wait for clean disconnect
                        
                        // Set WiFi to station mode for scanning
                        WiFi.mode(WIFI_STA);
                        delay(100);
                        
                        int n = WiFi.scanNetworks();
                        Serial.print("Scan completed, found ");
                        Serial.print(n);
                        Serial.println(" networks");
                        
                        wifi_scan_result_t result;
                        
                        // Check for scan errors (negative return values)
                        if (n < 0) {
                            Serial.print("WiFi scan failed with error code: ");
                            Serial.println(n);
                            Serial.println("Retrying scan in 1 second...");
                            delay(1000);
                            
                            // Retry scan once
                            n = WiFi.scanNetworks();
                            Serial.print("Retry scan found ");
                            Serial.print(n);
                            Serial.println(" networks");
                        }
                        
                        if (n < 0) {
                            Serial.println("Scan failed after retry, returning empty results");
                            result.network_count = 0;
                        } else {
                            result.network_count = (n > 9) ? 9 : n;
                            
                            for (int i = 0; i < result.network_count; i++) {
                                strncpy(result.ssids[i], WiFi.SSID(i).c_str(), 32);
                                result.ssids[i][32] = '\0';
                                result.rssi[i] = WiFi.RSSI(i);
                                result.encrypted[i] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
                            }
                        }
                        
                        // Re-enable auto-reconnect for normal operation
                        WiFi.setAutoReconnect(true);
                        
                        if (wifi_scan_result_queue != NULL) {
                            if (xQueueSend(wifi_scan_result_queue, &result, 0) == pdTRUE) {
                                Serial.println("Scan results sent to queue successfully");
                            } else {
                                Serial.println("Failed to send scan results to queue");
                            }
                        }
                        break;
                    }
                    case WIFI_CONNECT:
                        Serial.println("Connecting to WiFi...");
                        WiFi.begin(command.ssid, command.password);
                        break;
                    case WIFI_DISCONNECT:
                        Serial.println("Disconnecting from WiFi...");
                        WiFi.disconnect(true);
                        break;
                    case WIFI_STOP_SCAN:
                        Serial.println("Stopping WiFi scan...");
                        break;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    // Status checker callback
    static void wifiStatusCheckerCB(lv_timer_t *timer) {
        lv_obj_t* status_label = (lv_obj_t*)timer->user_data;
        int attempts = (int)lv_obj_get_user_data(status_label);
        attempts++;

        if (WiFi.status() == WL_CONNECTED) {
            lv_label_set_text_fmt(status_label, "Connected!\nIP: %s", WiFi.localIP().toString().c_str());
            
            preferences.begin("wifi-creds", false);
            preferences.putString("ssid", current_ssid);
            preferences.putString("password", current_password);
            preferences.end();
            Serial.println("WiFi credentials saved.");

            lv_timer_del(timer);
            wifi_status_timer = NULL;
            return;
        }

        if (attempts > 30) { // 15-second timeout
            lv_label_set_text(status_label, "Connection Failed!");
            WiFi.disconnect(true);
            lv_timer_del(timer);
            wifi_status_timer = NULL;
            return;
        }

        lv_obj_set_user_data(status_label, (void*)attempts);
        delay(1);
    }
    
    // Main status updater callback
    static void mainStatusUpdaterCB(lv_timer_t *timer) {
        if (ap_mode_active) {
            if (main_wifi_status_label != NULL && lv_obj_is_valid(main_wifi_status_label)) {
                lv_label_set_text(main_wifi_status_label, "AP Mode Active");
                lv_obj_set_style_text_color(main_wifi_status_label, lv_color_hex(0x4CAF50), 0);
            }
            // Relay status will be handled by external modules
            return;
        }

        if (main_wifi_status_label != NULL && lv_obj_is_valid(main_wifi_status_label)) {
            if (WiFi.status() == WL_CONNECTED) {
                String status_text = String(LV_SYMBOL_WIFI) + " " + WiFi.SSID();
                lv_label_set_text(main_wifi_status_label, status_text.c_str());
                lv_obj_set_style_text_color(main_wifi_status_label, lv_color_hex(0x00FF00), 0);
                
                // NWC and Bitcoin price fetching will be handled by external modules
            } else {
                unsigned long current_time = millis();
                if (wifi_connection_attempted && (current_time - wifi_connect_start_time > WIFI_CONNECT_TIMEOUT)) {
                    lv_label_set_text(main_wifi_status_label, LV_SYMBOL_WIFI " Timeout");
                    lv_obj_set_style_text_color(main_wifi_status_label, lv_color_hex(0xFF5722), 0);
                } else {
                    lv_label_set_text(main_wifi_status_label, LV_SYMBOL_WIFI " Not Connected");
                    lv_obj_set_style_text_color(main_wifi_status_label, lv_color_hex(0x9E9E9E), 0);
                }
                
                // NWC disconnection will be handled by external modules
            }
        }
        
        if (status_callback) {
            status_callback(WiFi.status() == WL_CONNECTED, 
                           WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
        }
        
        delay(1);
    }
    
    void init() {
        WiFi.mode(WIFI_STA);
        timeClient.begin();
        timeClient.setTimeOffset(0);
        
        // Create queues
        wifi_command_queue = xQueueCreate(10, sizeof(wifi_command_t));
        wifi_scan_result_queue = xQueueCreate(5, sizeof(wifi_scan_result_t));
        
        createTask();
        createStatusTimer();
        
        // Load NWC URL
        loadNWCUrl();
        
        // Try to connect to saved WiFi if not in AP mode
        if (!ap_mode_active) {
            preferences.begin("wifi-creds", true);
            String saved_ssid = preferences.getString("ssid", "");
            String saved_pass = preferences.getString("password", "");
            preferences.end();

            if (saved_ssid.length() > 0) {
                Serial.println("Found saved WiFi credentials.");
                Serial.print("Connecting to ");
                Serial.println(saved_ssid);
                startConnection(saved_ssid.c_str(), saved_pass.c_str());
            }
        }
    }
    
    void cleanup() {
        deleteTask();
        deleteStatusTimer();
        
        if (wifi_command_queue) {
            vQueueDelete(wifi_command_queue);
            wifi_command_queue = NULL;
        }
        
        if (wifi_scan_result_queue) {
            vQueueDelete(wifi_scan_result_queue);
            wifi_scan_result_queue = NULL;
        }
    }
    
    void processLoop() {
        if (isAPModeActive()) {
            dns_server.processNextRequest();
            ap_server.handleClient();
        }
    }
    
    void startConnection(const char* ssid, const char* password) {
        strncpy(current_ssid, ssid, sizeof(current_ssid) - 1);
        current_ssid[sizeof(current_ssid) - 1] = '\0';
        strncpy(current_password, password, sizeof(current_password) - 1);
        current_password[sizeof(current_password) - 1] = '\0';
        
        if (wifi_command_queue != NULL) {
            wifi_command_t command;
            command.type = WIFI_CONNECT;
            strncpy(command.ssid, ssid, sizeof(command.ssid) - 1);
            command.ssid[sizeof(command.ssid) - 1] = '\0';
            strncpy(command.password, password, sizeof(command.password) - 1);
            command.password[sizeof(command.password) - 1] = '\0';
            xQueueSend(wifi_command_queue, &command, 0);
        }
        
        wifi_connect_start_time = millis();
        wifi_connection_attempted = true;
    }
    
    void disconnect() {
        if (wifi_command_queue != NULL) {
            wifi_command_t command;
            command.type = WIFI_DISCONNECT;
            xQueueSend(wifi_command_queue, &command, 0);
        }
    }
    
    bool isConnected() {
        return WiFi.status() == WL_CONNECTED;
    }
    
    String getSSID() {
        return WiFi.SSID();
    }
    
    String getLocalIP() {
        return WiFi.localIP().toString();
    }
    
    wl_status_t getStatus() {
        return WiFi.status();
    }
    
    void startScan() {
        Serial.println("Scanning for WiFi networks...");
        if (UI::getWiFiList()) {
            lv_obj_clean(UI::getWiFiList());
            lv_list_add_text(UI::getWiFiList(), "Scanning for networks...");
        }
        
        if (wifi_command_queue != NULL) {
            wifi_command_t command;
            command.type = WIFI_SCAN;
            if (xQueueSend(wifi_command_queue, &command, 0) == pdTRUE) {
                Serial.println("Scan command sent to WiFi task successfully");
            } else {
                Serial.println("Failed to send scan command to WiFi task");
            }
        }
        
        wifi_scan_timer = lv_timer_create([](lv_timer_t *timer) {
            if (processScanResults()) {
                lv_timer_del(timer);
                wifi_scan_timer = NULL;
            }
        }, 500, NULL);
    }
    
    void stopScanning() {
        Serial.println("Stopping WiFi scanning...");
        
        if (wifi_scan_timer != NULL) {
            lv_timer_del(wifi_scan_timer);
            wifi_scan_timer = NULL;
        }
        
        if (wifi_command_queue != NULL) {
            wifi_command_t command;
            command.type = WIFI_STOP_SCAN;
            xQueueSend(wifi_command_queue, &command, 0);
        }
        
        if (wifi_scan_result_queue != NULL) {
            wifi_scan_result_t result;
            while (xQueueReceive(wifi_scan_result_queue, &result, 0) == pdTRUE) {
                // Clear the queue
            }
        }
    }
    
    bool processScanResults() {
        if (wifi_scan_result_queue == NULL) {
            return false;
        }
        
        wifi_scan_result_t result;
        if (xQueueReceive(wifi_scan_result_queue, &result, 0)) {
            Serial.print("Found ");
            Serial.print(result.network_count);
            Serial.println(" networks.");
            
            if (UI::getWiFiList()) {
                lv_obj_clean(UI::getWiFiList());
                
                if (result.network_count == 0) {
                    lv_list_add_text(UI::getWiFiList(), "No networks found");
                } else {
                    wifi_ssids.clear();
                    wifi_ssids.reserve(result.network_count);
                    
                    for (int i = 0; i < result.network_count; i++) {
                        String ssid = String(result.ssids[i]);
                        wifi_ssids.push_back(ssid);
                        
                        String rssi = String(result.rssi[i]);
                        String security = result.encrypted[i] ? "Lck" : " ";
                        String item_text = ssid + " (" + rssi + " dBm) " + security;
                        
                        Serial.print("Adding network: ");
                        Serial.println(item_text);
                        
                        lv_obj_t* list_btn = lv_list_add_btn(UI::getWiFiList(), NULL, item_text.c_str());
                        lv_obj_add_event_cb(list_btn, connectEventHandler, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
                        
                        if (i % 3 == 0) {
                            delay(1);
                        }
                    }
                }
            }
            return true;
        }
        return false;
    }
    
    void saveCredentials(const char* ssid, const char* password) {
        preferences.begin("wifi-creds", false);
        
        int count = preferences.getInt("count", 0);
        String ssid_key = "ssid_" + String(count);
        String pass_key = "pass_" + String(count);
        
        preferences.putString(ssid_key.c_str(), ssid);
        preferences.putString(pass_key.c_str(), password);
        preferences.putInt("count", count + 1);
        
        preferences.end();
        Serial.printf("Saved WiFi network %d: %s\n", count, ssid);
    }
    
    bool findSavedCredentials(const char* ssid, char* password, size_t password_size) {
        preferences.begin("wifi-creds", true);
        
        int count = preferences.getInt("count", 0);
        
        for (int i = 0; i < count; i++) {
            String ssid_key = "ssid_" + String(i);
            String saved_ssid = preferences.getString(ssid_key.c_str(), "");
            
            if (saved_ssid == ssid) {
                String pass_key = "pass_" + String(i);
                String saved_password = preferences.getString(pass_key.c_str(), "");
                strncpy(password, saved_password.c_str(), password_size - 1);
                password[password_size - 1] = '\0';
                preferences.end();
                return true;
            }
        }
        
        preferences.end();
        return false;
    }
    
    void loadAllNetworks() {
        preferences.begin("wifi-creds", true);
        
        int count = preferences.getInt("count", 0);
        Serial.printf("Found %d saved WiFi networks\n", count);
        
        for (int i = 0; i < count; i++) {
            String ssid_key = "ssid_" + String(i);
            String ssid = preferences.getString(ssid_key.c_str(), "");
            
            if (ssid.length() > 0) {
                Serial.printf("Network %d: %s\n", i, ssid.c_str());
            }
        }
        
        preferences.end();
    }
    
    void removeNetwork(const char* ssid) {
        preferences.begin("wifi-creds", false);
        
        int count = preferences.getInt("count", 0);
        
        for (int i = 0; i < count; i++) {
            String ssid_key = "ssid_" + String(i);
            String saved_ssid = preferences.getString(ssid_key.c_str(), "");
            
            if (saved_ssid == ssid) {
                for (int j = i; j < count - 1; j++) {
                    String current_ssid_key = "ssid_" + String(j);
                    String current_pass_key = "pass_" + String(j);
                    String next_ssid_key = "ssid_" + String(j + 1);
                    String next_pass_key = "pass_" + String(j + 1);
                    
                    String next_ssid = preferences.getString(next_ssid_key.c_str(), "");
                    String next_password = preferences.getString(next_pass_key.c_str(), "");
                    
                    preferences.putString(current_ssid_key.c_str(), next_ssid);
                    preferences.putString(current_pass_key.c_str(), next_password);
                }
                
                preferences.putInt("count", count - 1);
                Serial.printf("Removed WiFi network: %s\n", ssid);
                break;
            }
        }
        
        preferences.end();
    }
    
    void startAPMode() {
        if (ap_mode_active) {
            Serial.println("AP mode already active");
            return;
        }
        
        Serial.println("Starting Access Point mode...");
        
        if (NWC::isInitialized()) {
            // webSocket.disconnect();
            // webSocket.setReconnectInterval(0);
            // NWC disconnection handled by NWC module
            Serial.println("Disconnected from relay and disabled reconnection");
        }
        
        WiFi.disconnect(true);
        delay(1000);
        
        WiFi.mode(WIFI_AP);
        
        IPAddress local_IP;
        local_IP.fromString(ap_ip);
        IPAddress gateway(192, 168, 4, 1);
        IPAddress subnet(255, 255, 255, 0);
        
        bool ap_started = WiFi.softAP(ap_ssid, Settings::getAPPassword().c_str());
        if (!ap_started) {
            Serial.println("Failed to start AP");
            return;
        }
        
        WiFi.softAPConfig(local_IP, gateway, subnet);
        dns_server.start(53, "*", local_IP);
        
        ap_server.on("/", HTTP_GET, handleAPRoot);
        ap_server.on("/config", HTTP_POST, handleAPConfig);
        ap_server.on("/current-url", HTTP_GET, []() {
            ap_server.send(200, "text/plain", NWC::getPairingUrl());
        });
        ap_server.onNotFound([]() {
            ap_server.sendHeader("Location", "http://" + String(ap_ip), true);
            ap_server.send(302, "text/plain", "");
        });
        
        ap_server.begin();
        ap_mode_active = true;
        
        Serial.println("Access Point started successfully");
        updateSettingsScreenForAPMode();
        
        UI::showMessage("NWC Pairing Code", "Connect to the WiFi hotspot below to set your NWC pairing code.\nSSID: " + String(ap_ssid) + "\nPassword: " + Settings::getAPPassword() + "\nIP: " + String(ap_ip));
    }
    
    void stopAPMode() {
        if (!ap_mode_active) {
            return;
        }
        
        ap_server.close();
        dns_server.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        ap_mode_active = false;
        
        Serial.println("Access Point stopped");
        
        // Try to reconnect to saved WiFi
        preferences.begin("wifi-creds", true);
        String saved_ssid = preferences.getString("ssid", "");
        String saved_pass = preferences.getString("password", "");
        preferences.end();

        if (saved_ssid.length() > 0) {
            Serial.println("Attempting to reconnect to saved WiFi: " + saved_ssid);
            startConnection(saved_ssid.c_str(), saved_pass.c_str());
        }
    }
    
    bool isAPModeActive() {
        return ap_mode_active;
    }
    
    String getAPSSID() {
        return String(ap_ssid);
    }
    
    String getAPPassword() {
        return Settings::getAPPassword();
    }
    
    String getAPIP() {
        return String(ap_ip);
    }
    
    void loadNWCUrl() {
        preferences.begin("nwc-config", true);
        String saved_url = preferences.getString("nwc_url", "");
        preferences.end();
        
        if (saved_url.length() > 0) {
            NWC::setPairingUrl(saved_url);
            Serial.println("Loaded NWC URL from preferences: " + saved_url);
        } else {
            Serial.println("No saved NWC URL found, using default");
        }
    }
    
    void saveNWCUrl(const String& url) {
        preferences.begin("nwc-config", false);
        preferences.putString("nwc_url", url);
        preferences.end();
        Serial.println("Saved NWC URL to preferences: " + url);
    }
    
    String getNWCUrl() {
        return NWC::getPairingUrl();
    }
    
    void setNWCUrl(const String& url) {
        NWC::setPairingUrl(url);
        saveNWCUrl(url);
    }
    
    void updateStatus() {
        // Trigger status update
        mainStatusUpdaterCB(NULL);
    }
    
    void setStatusLabel(lv_obj_t* label) {
        wifi_status_label = label;
    }
    
    void setMainStatusLabel(lv_obj_t* label) {
        main_wifi_status_label = label;
    }
    
    // Event handlers
    void scanEventHandler(lv_event_t* e) {
        if (e != NULL) {
            lv_event_code_t code = lv_event_get_code(e);
            if (code != LV_EVENT_CLICKED) return;
        }
        startScan();
    }
    
    void connectEventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED) {
            // Reset activity timer on WiFi network selection
            App::resetActivityTimer();
            
            int index = (int)(uintptr_t)lv_event_get_user_data(e);
            
            if (index < wifi_ssids.size()) {
                const char* ssid = wifi_ssids[index].c_str();
                Serial.print("Selected WiFi network: ");
                Serial.println(ssid);
                UI::createWiFiPasswordScreen(ssid);
            } else {
                Serial.println("Invalid WiFi network index");
            }
        }
    }
    
    void passwordKBEventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* kb = lv_event_get_target(e);
        
        if (code == LV_EVENT_READY) {
            lv_obj_t* ta = lv_keyboard_get_textarea(kb);
            const char* password = lv_textarea_get_text(ta);
            strncpy(current_password, password, sizeof(current_password) - 1);
            current_password[sizeof(current_password) - 1] = '\0';
            
            Serial.print("Attempting to connect to ");
            Serial.print(current_ssid);
            Serial.print(" with password: ");
            Serial.println(password);

            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ta, LV_OBJ_FLAG_HIDDEN);

            if (wifi_status_label) {
                lv_obj_clear_flag(wifi_status_label, LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(wifi_status_label, "Connecting...");
                lv_obj_align(wifi_status_label, LV_ALIGN_CENTER, 0, 0);
                lv_obj_set_user_data(wifi_status_label, (void*)0);
            }

            startConnection(current_ssid, current_password);

            if (wifi_status_label) {
                wifi_status_timer = lv_timer_create(wifiStatusCheckerCB, 500, wifi_status_label);
            }
        } else if (code == LV_EVENT_CANCEL) {
            UI::loadScreen((UI::screen_state_t)2); // SCREEN_WIFI
        }
    }
    
    void passwordBackEventHandler(lv_event_t* e) {
        UI::loadScreen((UI::screen_state_t)2); // SCREEN_WIFI
    }
    
    void launchAPModeEventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED) {
            Settings::showPinVerificationScreen();
        }
    }
    
    void exitAPModeEventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED) {
            Serial.println("Exiting Access Point mode");
            stopAPMode();
            UI::loadScreen((UI::screen_state_t)1); // SCREEN_SETTINGS
        }
    }
    
    void createTask() {
        xTaskCreatePinnedToCore(
            wifiTask,
            "WiFiTask",
            4096,
            NULL,
            1,
            &wifi_task_handle,
            0
        );
    }
    
    void deleteTask() {
        if (wifi_task_handle != NULL) {
            vTaskDelete(wifi_task_handle);
            wifi_task_handle = NULL;
        }
    }
    
    TaskHandle_t getTaskHandle() {
        return wifi_task_handle;
    }
    
    QueueHandle_t getCommandQueue() {
        return wifi_command_queue;
    }
    
    QueueHandle_t getScanResultQueue() {
        return wifi_scan_result_queue;
    }
    
    void setCurrentCredentials(const char* ssid, const char* password) {
        strncpy(current_ssid, ssid, sizeof(current_ssid) - 1);
        current_ssid[sizeof(current_ssid) - 1] = '\0';
        strncpy(current_password, password, sizeof(current_password) - 1);
        current_password[sizeof(current_password) - 1] = '\0';
    }
    
    void getCurrentCredentials(char* ssid, char* password) {
        strcpy(ssid, current_ssid);
        strcpy(password, current_password);
    }
    
    std::vector<String>& getSSIDList() {
        return wifi_ssids;
    }
    
    void setStatusCallback(wifi_status_callback_t callback) {
        status_callback = callback;
    }
    
    NTPClient& getNTPClient() {
        return timeClient;
    }
    
    void createStatusTimer() {
        lv_timer_create(mainStatusUpdaterCB, 1000, NULL);
    }
    
    void deleteStatusTimer() {
        // Timers are automatically cleaned up when objects are deleted
    }
    
    void handleAPRoot() {
        String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>NWC Pairing Code Settings</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .container { max-width: 500px; margin: 0 auto; }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        input[type="text"] { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; }
        button { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; }
        button:hover { background-color: #45a049; }
        .info { background-color: #e7f3ff; padding: 10px; border-radius: 4px; margin-bottom: 20px; word-wrap: break-word; }
    </style>
</head>
<body>
    <div class="container">
        <h1>NWC Pairing Code Settings</h1>
        <p>Set the NWC pairing code for your NWC POS device in the NWC Pairing URL field below and click the Save Configuration button.</p>
        
        <div class="info">
            <strong>Current NWC Pairing Code:</strong><br>
            <span id="current-url">Loading...</span>
        </div>
        
        <form action="/config" method="post">
            <div class="form-group">
                <label for="nwc_url">NWC Pairing URL:</label>
                <input type="text" id="nwc_url" name="nwc_url" placeholder="nostr+walletconnect://..." required>
            </div>
            <button type="submit">Save Configuration</button>
        </form>
    </div>
    
    <script>
        window.onload = function() {
            fetch('/current-url')
                .then(response => response.text())
                .then(url => {
                    document.getElementById('current-url').textContent = url || 'Not configured';
                    document.getElementById('nwc_url').value = url || '';
                });
        };
    </script>
</body>
</html>
        )";
        
        ap_server.send(200, "text/html", html);
    }
    
    void handleAPConfig() {
        if (ap_server.hasArg("nwc_url")) {
            String nwc_url = ap_server.arg("nwc_url");
            NWC::setPairingUrl(nwc_url);
            
            // NWC connection restart will be handled by external modules
            Serial.println("NWC URL updated - restart will be handled externally");
            
            String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Configuration Saved</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; text-align: center; }
        .success { color: #4CAF50; font-size: 18px; margin: 20px 0; }
        .back-btn { background-color: #2196F3; color: white; padding: 10px 20px; text-decoration: none; border-radius: 4px; display: inline-block; margin-top: 20px; }
    </style>
</head>
<body>
    <div class="success">✓ Configuration saved successfully!</div>
    <p>The NWC pairing URL has been updated.</p>
    <a href="/" class="back-btn">Back to Configuration</a>
</body>
</html>
            )";
            
            ap_server.send(200, "text/html", html);
            Serial.println("NWC URL updated: " + nwc_url);
        } else {
            ap_server.send(400, "text/plain", "Missing NWC URL parameter");
        }
    }
    
    void updateSettingsScreenForAPMode() {
        UI::loadScreen((UI::screen_state_t)1); // SCREEN_SETTINGS
    }
}