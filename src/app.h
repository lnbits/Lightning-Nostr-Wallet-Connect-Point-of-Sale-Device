#pragma once

/**
 * App.h - Main Application Coordinator
 * 
 * This header provides a unified interface to all application modules
 * and coordinates initialization, cleanup, and inter-module communication
 * for the NWC Point of Sale device.
 */

#include <Arduino.h>
#include "driver/gpio.h"
#include "settings.h"
#include "display.h"
#include "wifi.h"
#include "ui.h"
#include "nwc.h"

namespace App {
    /**
     * Application State Management
     */
    typedef enum {
        APP_STATE_INITIALIZING,
        APP_STATE_READY,
        APP_STATE_ERROR,
        APP_STATE_UPDATING
    } app_state_t;
    
    /**
     * Core Application Functions
     */
    void init();                    // Initialize all modules
    void cleanup();                 // Cleanup all modules
    void run();                     // Main application loop
    
    /**
     * State Management
     */
    app_state_t getState();
    void setState(app_state_t state);
    String getStateString();
    
    /**
     * Error Handling
     */
    void handleError(const String& module, const String& error);
    void clearError();
    String getLastError();
    
    /**
     * Inter-Module Communication
     */
    void notifyWiFiStatusChanged(bool connected);
    void notifyNWCStatusChanged(bool connected);
    void notifyPricesUpdated();
    void notifyInvoiceCreated(const String& invoice);
    void notifyPaymentReceived();
    
    /**
     * Configuration Management
     */
    bool loadConfiguration();
    bool saveConfiguration();
    void resetToDefaults();
    
    /**
     * System Information
     */
    String getVersion();
    String getBuildInfo();
    void printSystemInfo();
    
    /**
     * Power Management
     */
    void enterSleepMode();
    void exitSleepMode();
    
    /**
     * Deep Sleep Management
     */
    void initSleepMode();
    void cleanupSleepMode();
    void resetActivityTimer();
    void sleepCheckCallback(lv_timer_t* timer);
    void setSleepEnabled(bool enabled);
    bool isSleepEnabled();
    unsigned long getInactiveTime();
    unsigned long getSleepTimeout();
    
    /**
     * Module Health Monitoring
     */
    bool checkModuleHealth();
    void reportModuleStatus();
    
    /**
     * Update Management
     */
    void checkForUpdates();
    bool isUpdateAvailable();
    void performUpdate();
    
    /**
     * Diagnostic Functions
     */
    void runDiagnostics();
    void generateStatusReport();
    
    /**
     * Event System
     */
    typedef void (*app_event_callback_t)(const String& event, const String& data);
    void setEventCallback(app_event_callback_t callback);
    void fireEvent(const String& event, const String& data);
    
    /**
     * Constants
     */
    namespace Config {
        const String VERSION = "v1.0.0";
        const String BUILD_DATE = __DATE__ " " __TIME__;
        const unsigned long HEALTH_CHECK_INTERVAL = 30000; // 30 seconds
        const unsigned long STATUS_REPORT_INTERVAL = 300000; // 5 minutes
        
        // Deep Sleep Configuration
        const unsigned long SLEEP_TIMEOUT = 30 * 1000; // 30 seconds
        const gpio_num_t WAKE_PIN = GPIO_NUM_10; // GPIO for wake up
        const unsigned long SLEEP_CHECK_INTERVAL = 1000; // Check every second
    }
}