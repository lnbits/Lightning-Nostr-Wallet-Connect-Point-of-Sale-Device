#include "app.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

namespace App
{
    // Application state
    static app_state_t current_state = APP_STATE_INITIALIZING;
    static String last_error = "";
    static app_event_callback_t event_callback = nullptr;
    static unsigned long last_health_check = 0;
    static unsigned long last_status_report = 0;

    // Sleep management
    static unsigned long last_activity_time = 0;
    static bool sleep_enabled = true;
    static lv_timer_t *sleep_check_timer = NULL;
    static bool in_light_sleep = false;
    static bool backlight_off = false;
    
    // Wake grace period management
    static unsigned long wake_grace_start_time = 0;
    static bool in_wake_grace_period = false;

    // Sleep timing constants
    static const unsigned long LIGHT_SLEEP_TIMEOUT = 10 * 1000; // 10 seconds
    static const unsigned long DEEP_SLEEP_TIMEOUT = 2 * 60 * 1000; // 2 minutes

    void init()
    {
        Serial.println("=== NWC Point of Sale Device Initializing ===");
        Serial.println("Version: " + Config::VERSION);
        Serial.println("Build Date: " + Config::BUILD_DATE);

        setState(APP_STATE_INITIALIZING);

        try
        {
            // Initialize modules in dependency order
            Serial.println("Initializing Display module...");
            Display::init();

            Serial.println("Initializing Settings module...");
            Settings::init();

            Serial.println("Initializing WiFi Manager module...");
            WiFiManager::init();

            // Set up WiFi status callback
            WiFiManager::setStatusCallback([](bool connected, const char *status)
                                           { App::notifyWiFiStatusChanged(connected); });

            Serial.println("Initializing UI module...");
            UI::init();

            Serial.println("Initializing NWC module...");
            NWC::init();

            // Set up NWC status callback
            NWC::setStatusCallback([](bool connected, const String &status)
                                   { App::notifyNWCStatusChanged(connected); });

            // Load the initial screen
            UI::loadScreen(UI::SCREEN_KEYPAD);

            // Check if WiFi is already connected and trigger NWC connection if needed
            if (WiFiManager::isConnected())
            {
                Serial.println("WiFi already connected during initialization - connecting to NWC relay");
                notifyWiFiStatusChanged(true);
            }

            // Initialize sleep functionality
            Serial.println("Initializing sleep management...");
            initSleepMode();

            setState(APP_STATE_READY);
            Serial.println("=== Application initialized successfully ===");

            fireEvent("app_initialized", "success");
        }
        catch (const std::exception &e)
        {
            setState(APP_STATE_ERROR);
        }
    }

    void cleanup()
    {
        Serial.println("=== Application cleanup starting ===");

        // Cleanup sleep management
        cleanupSleepMode();

        // Cleanup modules in reverse dependency order
        NWC::cleanup();
        UI::cleanup();
        WiFiManager::cleanup();
        Settings::cleanup();
        Display::cleanup();

        setState(APP_STATE_INITIALIZING);
        Serial.println("=== Application cleanup completed ===");

        fireEvent("app_cleanup", "completed");
    }

    void run()
    {
        static bool first_run = true;
        if (first_run) {
            Serial.println("=== App::run() started ===");
            first_run = false;
        }
        
        unsigned long current_time = millis();

        // Process WiFi AP mode events (with error handling)
        try {
            WiFiManager::processLoop();
        } catch (...) {
            Serial.println("ERROR: WiFiManager::processLoop() threw exception");
        }

        // Process NWC WebSocket events (with error handling)
        try {
            NWC::processLoop();
        } catch (...) {
            Serial.println("ERROR: NWC::processLoop() threw exception");
        }

        // Update NWC time synchronization (with error handling)
        try {
            NWC::updateTime();
        } catch (...) {
            Serial.println("ERROR: NWC::updateTime() threw exception");
        }

        // Periodic health checks
        if (current_time - last_health_check >= Config::HEALTH_CHECK_INTERVAL)
        {
            checkModuleHealth();
            last_health_check = current_time;
        }

        // Periodic status reports
        if (current_time - last_status_report >= Config::STATUS_REPORT_INTERVAL)
        {
            reportModuleStatus();
            last_status_report = current_time;
        }

        // Handle reconnection attempts (with error handling)
        try {
            NWC::attemptReconnectionIfNeeded();
        } catch (...) {
            Serial.println("ERROR: NWC::attemptReconnectionIfNeeded() threw exception");
        }

        // Small delay to prevent watchdog triggers
        delay(1);
    }

    app_state_t getState()
    {
        return current_state;
    }

    void setState(app_state_t state)
    {
        if (current_state != state)
        {
            current_state = state;
            Serial.println("App state changed to: " + getStateString());
            fireEvent("state_changed", getStateString());
        }
    }

    String getStateString()
    {
        switch (current_state)
        {
        case APP_STATE_INITIALIZING:
            return "Initializing";
        case APP_STATE_READY:
            return "Ready";
        case APP_STATE_ERROR:
            return "Error";
        case APP_STATE_UPDATING:
            return "Updating";
        default:
            return "Unknown";
        }
    }

    void handleError(const String &module, const String &error)
    {
        last_error = module + ": " + error;
        Serial.println("ERROR - " + last_error);

        // Show error message to user
        UI::showMessage("Error", last_error);

        fireEvent("error", last_error);
    }

    void clearError()
    {
        last_error = "";
        if (current_state == APP_STATE_ERROR)
        {
            setState(APP_STATE_READY);
        }
    }

    String getLastError()
    {
        return last_error;
    }

    void notifyWiFiStatusChanged(bool connected)
    {
        static bool last_wifi_status = false;

        // Prevent duplicate notifications
        if (last_wifi_status == connected)
        {
            return;
        }
        last_wifi_status = connected;

        Serial.println("WiFi status changed: " + String(connected ? "Connected" : "Disconnected"));

        if (connected)
        {
            // WiFi connected - attempt NWC connection
            if (!NWC::isConnected())
            {
                Serial.println("WiFi connected, attempting NWC relay connection...");
                NWC::connectToRelay();
            }

            // Fetch Bitcoin prices
            NWC::fetchBitcoinPrices();
        }
        else
        {
            // WiFi disconnected - notify NWC module
            Serial.println("WiFi disconnected, disconnecting NWC...");
            NWC::disconnect();
        }

        fireEvent("wifi_status", connected ? "connected" : "disconnected");
    }

    void notifyNWCStatusChanged(bool connected)
    {
        static bool last_nwc_status = false;

        // Prevent duplicate notifications
        if (last_nwc_status == connected)
        {
            return;
        }
        last_nwc_status = connected;

        Serial.println("NWC status changed: " + String(connected ? "Connected" : "Disconnected"));
        NWC::updateRelayStatusDisplay(connected);
        fireEvent("nwc_status", connected ? "connected" : "disconnected");
    }

    void notifyPricesUpdated()
    {
        Serial.println("Bitcoin prices updated");
        fireEvent("prices_updated", "success");
    }

    void notifyInvoiceCreated(const String &invoice)
    {
        Serial.println("Invoice created: " + invoice.substring(0, 20) + "...");
        NWC::setCurrentInvoice(invoice);
        UI::createInvoiceOverlay();
        fireEvent("invoice_created", invoice);
    }

    void notifyPaymentReceived()
    {
        Serial.println("Payment received!");
        UI::closeInvoiceOverlay();
        UI::showMessage("Payment Received", "Transaction completed successfully!");
        fireEvent("payment_received", "success");
    }

    bool loadConfiguration()
    {
        Serial.println("Loading application configuration...");

        // Configuration is loaded by individual modules
        // This function could coordinate any cross-module configuration

        return true;
    }

    bool saveConfiguration()
    {
        Serial.println("Saving application configuration...");

        // Configuration is saved by individual modules
        // This function could coordinate any cross-module configuration

        return true;
    }

    void resetToDefaults()
    {
        Serial.println("Resetting to default configuration...");

        Settings::resetToDefaults();

        fireEvent("reset_defaults", "completed");
    }

    String getVersion()
    {
        return Config::VERSION;
    }

    String getBuildInfo()
    {
        return Config::BUILD_DATE;
    }

    void printSystemInfo()
    {
        Serial.println("=== System Information ===");
        Serial.println("Version: " + getVersion());
        Serial.println("Build Date: " + getBuildInfo());
        Serial.println("Free Heap: " + String(ESP.getFreeHeap()));
        Serial.println("Chip Model: " + String(ESP.getChipModel()));
        Serial.println("Chip Revision: " + String(ESP.getChipRevision()));
        Serial.println("CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
        Serial.println("Flash Size: " + String(ESP.getFlashChipSize()));
        Serial.println("==========================");
    }

    void enterSleepMode()
    {
        Serial.println("=== Entering deep sleep mode ===");

        // Disable sleep monitoring timer
        if (sleep_check_timer)
        {
            lv_timer_del(sleep_check_timer);
            sleep_check_timer = NULL;
        }

        // Close any active invoice overlays and stop timers
        if (UI::isInvoiceProcessing())
        {
            UI::closeInvoiceOverlay();
        }

        // Stop all NWC timers and disconnect
        NWC::stopInvoiceLookupTimer();
        NWC::stopInvoiceNotificationWatchdog();

        // Turn off display backlight for power saving
        Display::turnOffBacklight();

        // Set Config::WAKE_PIN as input with pulldown (to default LOW)
        // pinMode(Config::WAKE_PIN, INPUT);
        // rtc_gpio_pulldown_en(Config::WAKE_PIN);
        // rtc_gpio_pullup_dis(Config::WAKE_PIN);

        // // Enable EXT1 wakeup when GPIO12 goes HIGH
        // esp_sleep_enable_ext1_wakeup(1ULL << Config::WAKE_PIN, ESP_EXT1_WAKEUP_ANY_HIGH);

        // Serial.println("Going to deep sleep. Wake when " + String(Config::WAKE_PIN) + " goes HIGH.");
        Serial.flush();
        esp_deep_sleep_start();
    }

    void enterLightSleepMode()
    {
        Serial.println("=== Entering light sleep mode (backlight off) ===");
        
        in_light_sleep = true;
        backlight_off = true;
        
        // Turn off display backlight
        Display::turnOffBacklight();
        
        // Stop invoice lookup timer if no invoice is being processed
        if (!UI::isInvoiceProcessing()) {
            Serial.println("Stopping invoice lookup timer for light sleep");
            NWC::stopInvoiceLookupTimer();
            NWC::stopInvoiceNotificationWatchdog();
        }
        
        fireEvent("light_sleep", "entered");
    }

    void exitLightSleepMode()
    {
        Serial.println("=== Exiting light sleep mode ===");
        
        in_light_sleep = false;
        backlight_off = false;
        
        // Turn display backlight back on
        Display::turnOnBacklight();
        
        // Reset activity timer
        resetActivityTimer();
        
        fireEvent("light_sleep", "exited");
    }

    void exitSleepMode()
    {
        Serial.println("=== Exiting sleep mode (wake up) ===");

        // Print wake up reason
        esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
        switch (wakeup_reason)
        {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("Wake up caused by external signal using RTC_IO");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            Serial.println("Wake up caused by touchpad");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("Wake up caused by timer");
            break;
        default:
            Serial.println("Wake up was not caused by deep sleep: " + String(wakeup_reason));
            break;
        }

        // Turn display backlight back on
        Display::turnOnBacklight();
        backlight_off = false;

        // Reset activity timer
        resetActivityTimer();

        // Restart sleep monitoring
        if (sleep_enabled && sleep_check_timer == NULL)
        {
            sleep_check_timer = lv_timer_create(sleepCheckCallback, Config::SLEEP_CHECK_INTERVAL, NULL);
        }

        fireEvent("sleep_mode", "exited");
    }

    bool checkModuleHealth()
    {
        bool health_ok = true;

        // Check WiFi module health
        if (!WiFiManager::isConnected() && WiFiManager::getStatus() != WL_IDLE_STATUS)
        {
            Serial.println("WiFi module health check failed");
            health_ok = false;
        }

        // Check NWC module health
        if (NWC::isInitialized() && !NWC::isConnected())
        {
            Serial.println("NWC module health check warning - not connected");
            // This is a warning, not a failure
        }

        // Check UI module health
        if (UI::getCurrentScreen() < 0)
        {
            Serial.println("UI module health check failed");
            health_ok = false;
        }

        return health_ok;
    }

    void reportModuleStatus()
    {
        Serial.println("=== Module Status Report ===");
        Serial.println("WiFi: " + String(WiFiManager::isConnected() ? "Connected" : "Disconnected"));
        if (WiFiManager::isConnected())
        {
            Serial.println("  SSID: " + WiFiManager::getSSID());
            Serial.println("  IP: " + WiFiManager::getLocalIP());
        }
        Serial.println("NWC: " + String(NWC::isConnected() ? "Connected" : "Disconnected"));
        if (NWC::isConnected())
        {
            Serial.println("  Relay: " + NWC::getRelayUrl());
        }
        Serial.println("Current Screen: " + String(UI::getCurrentScreen()));
        Serial.println("Free Heap: " + String(ESP.getFreeHeap()));
        Serial.println("============================");
    }

    void checkForUpdates()
    {
        Serial.println("Checking for updates...");
        // Implementation would check for firmware updates
        fireEvent("update_check", "started");
    }

    bool isUpdateAvailable()
    {
        // Implementation would check if update is available
        return false;
    }

    void performUpdate()
    {
        Serial.println("Performing update...");
        setState(APP_STATE_UPDATING);
        // Implementation would perform firmware update
        fireEvent("update", "started");
    }

    void runDiagnostics()
    {
        Serial.println("=== Running Diagnostics ===");

        printSystemInfo();
        reportModuleStatus();

        // Test display
        UI::showMessage("Diagnostics", "Display test - press any key");

        // Test WiFi scan
        if (WiFiManager::isConnected())
        {
            Serial.println("WiFi diagnostic: PASS");
        }
        else
        {
            Serial.println("WiFi diagnostic: FAIL - Not connected");
        }

        // Test NWC connection
        if (NWC::isConnected())
        {
            Serial.println("NWC diagnostic: PASS");
        }
        else
        {
            Serial.println("NWC diagnostic: FAIL - Not connected");
        }

        Serial.println("=== Diagnostics Complete ===");
        fireEvent("diagnostics", "completed");
    }

    void generateStatusReport()
    {
        String report = "Status Report\\n";
        report += "=============\\n";
        report += "Version: " + getVersion() + "\\n";
        report += "State: " + getStateString() + "\\n";
        report += "WiFi: " + String(WiFiManager::isConnected() ? "OK" : "FAIL") + "\\n";
        report += "NWC: " + String(NWC::isConnected() ? "OK" : "FAIL") + "\\n";
        report += "Heap: " + String(ESP.getFreeHeap()) + "\\n";

        if (last_error.length() > 0)
        {
            report += "Last Error: " + last_error + "\\n";
        }

        Serial.println(report);
        fireEvent("status_report", report);
    }

    void setEventCallback(app_event_callback_t callback)
    {
        event_callback = callback;
    }

    void fireEvent(const String &event, const String &data)
    {
        if (event_callback)
        {
            event_callback(event, data);
        }

        // Log event
        Serial.println("EVENT: " + event + " - " + data);
    }

    // ========== Sleep Management Functions ==========

    void initSleepMode()
    {
        // Configure GPIO 12 as input with pulldown for wake button
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << Config::WAKE_PIN);
        io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);

        // Initialize activity timer
        resetActivityTimer();

        // Start sleep monitoring timer if enabled
        if (sleep_enabled)
        {
            sleep_check_timer = lv_timer_create(sleepCheckCallback, Config::SLEEP_CHECK_INTERVAL, NULL);
        }

        // Check if we're waking up from deep sleep
        esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
        if (wakeup_reason != ESP_SLEEP_WAKEUP_UNDEFINED)
        {
            exitSleepMode(); // Handle wake up
        }

        Serial.println("Sleep management initialized");
        Serial.println("Wake pin: GPIO " + String(Config::WAKE_PIN));
        Serial.println("Light sleep timeout: " + String(LIGHT_SLEEP_TIMEOUT / 1000) + " seconds");
        Serial.println("Deep sleep timeout: " + String(DEEP_SLEEP_TIMEOUT / 1000) + " seconds");
    }

    void cleanupSleepMode()
    {
        if (sleep_check_timer)
        {
            lv_timer_del(sleep_check_timer);
            sleep_check_timer = NULL;
        }
        Serial.println("Sleep management cleaned up");
    }

    void resetActivityTimer()
    {
        last_activity_time = millis();
        
        // If we were in light sleep, exit it
        if (in_light_sleep)
        {
            exitLightSleepMode();
        }
        
        Serial.println("Activity timer reset");
    }

    void sleepCheckCallback(lv_timer_t *timer)
    {
        if (!sleep_enabled)
        {
            return;
        }

        unsigned long current_time = millis();
        unsigned long inactive_time = current_time - last_activity_time;

        // Don't sleep if invoice processing is active
        if (UI::isInvoiceProcessing())
        {
            return;
        }

        // Don't sleep if WiFi is not connected (may be in setup mode)
        if (!WiFiManager::isConnected())
        {
            return;
        }

        // Only allow sleep from the keypad screen
        if (UI::getCurrentScreen() != UI::SCREEN_KEYPAD)
        {
            return;
        }

        // Check for deep sleep timeout (2 minutes)
        if (inactive_time >= DEEP_SLEEP_TIMEOUT)
        {
            Serial.println("Deep sleep timeout reached (" + String(inactive_time / 1000) + "s), entering deep sleep");
            enterSleepMode();
        }
        // Check for light sleep timeout (10 seconds) - only if not already in light sleep
        else if (!in_light_sleep && inactive_time >= LIGHT_SLEEP_TIMEOUT)
        {
            Serial.println("Light sleep timeout reached (" + String(inactive_time / 1000) + "s), entering light sleep");
            enterLightSleepMode();
        }
    }

    void setSleepEnabled(bool enabled)
    {
        sleep_enabled = enabled;

        if (enabled && sleep_check_timer == NULL)
        {
            sleep_check_timer = lv_timer_create(sleepCheckCallback, Config::SLEEP_CHECK_INTERVAL, NULL);
            Serial.println("Sleep mode enabled");
        }
        else if (!enabled && sleep_check_timer != NULL)
        {
            lv_timer_del(sleep_check_timer);
            sleep_check_timer = NULL;
            Serial.println("Sleep mode disabled");
        }
    }

    bool isSleepEnabled()
    {
        return sleep_enabled;
    }

    bool isInLightSleep()
    {
        return in_light_sleep;
    }

    bool isBacklightOff()
    {
        return backlight_off;
    }

    unsigned long getInactiveTime()
    {
        return millis() - last_activity_time;
    }

    unsigned long getLightSleepTimeout()
    {
        return LIGHT_SLEEP_TIMEOUT;
    }

    unsigned long getDeepSleepTimeout()
    {
        return DEEP_SLEEP_TIMEOUT;
    }

    // Touch wake handler - should be called from Display::touchpadRead when touch is detected
    void handleTouchWake()
    {
        if (in_light_sleep || backlight_off)
        {
            Serial.println("Touch detected - waking from light sleep");
            exitLightSleepMode();
            
            // Start wake grace period to prevent accidental keypad presses
            wake_grace_start_time = millis();
            in_wake_grace_period = true;
            Serial.println("Wake grace period started");
        }
    }
    
    // Check if device is currently in wake grace period
    bool isInWakeGracePeriod()
    {
        if (!in_wake_grace_period)
        {
            return false;
        }
        
        // Check if grace period has expired
        if (millis() - wake_grace_start_time >= Config::WAKE_GRACE_PERIOD)
        {
            in_wake_grace_period = false;
            Serial.println("Wake grace period ended");
            return false;
        }
        
        return true;
    }
}