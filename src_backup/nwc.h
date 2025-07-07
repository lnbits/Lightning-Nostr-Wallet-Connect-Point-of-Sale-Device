#pragma once

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <lvgl.h>
#include "nostr.h"
#include "Bitcoin.h"

namespace NWC {
    // Initialization and cleanup
    void init();
    void cleanup();
    
    // NWC connection management
    void setupData(const String& nwcPairingUrl);
    void connectToRelay();
    void restartConnection();
    void disconnect();
    bool isInitialized();
    bool isConnected();
    
    // Configuration management
    void loadUrlFromPreferences();
    void saveUrlToPreferences(const String& url);
    String getPairingUrl();
    void setPairingUrl(const String& url);
    
    // WebSocket event handling
    void websocketEvent(WStype_t type, uint8_t* payload, size_t length);
    void handleWebsocketMessage(void* arg, uint8_t* data, size_t len);
    void resetWebsocketFragmentState();
    
    // NWC protocol event handlers
    void handleNwcResponseEvent(uint8_t* data);
    void handleNwcNip04NotificationEvent(uint8_t* data);
    void handleNwcNip44NotificationEvent(uint8_t* data);
    
    // Invoice management
    void makeInvoice(float amount, const String& currency);
    void sendInvoiceLookupRequest();
    void startInvoiceNotificationWatchdog();
    void stopInvoiceNotificationWatchdog();
    void startInvoiceLookupTimer();
    void stopInvoiceLookupTimer();
    String getCurrentInvoice();
    void setCurrentInvoice(const String& invoice);
    
    // Bitcoin price services
    void fetchBitcoinPrices();
    void updatePricesFromJson(const String& jsonResponse);
    int calculateSatsFromAmount(float amount, const String& currency);
    long* getBitcoinPrices();
    bool arePricesLoaded();
    unsigned long getLastPriceUpdate();
    
    // Connection monitoring and reconnection
    void attemptReconnectionIfNeeded();
    void updateRelayStatusDisplay(bool connected);
    
    // Time synchronization
    void updateTime();
    unsigned long getUnixTimestamp();
    unsigned long getBootTimestamp();
    NTPClient& getTimeClient();
    
    // WebSocket management
    void sendPing();
    void processLoop();
    
    // Fragment handling
    bool isFragmentInProgress();
    void handleFragment(uint8_t* payload, size_t length, bool isBinary, bool isLast);
    
    // Timer callbacks
    void invoiceLookupTimerCallback(lv_timer_t* timer);
    void invoiceNotificationWatchdogCallback(lv_timer_t* timer);
    
    // Status callbacks
    typedef void (*nwc_status_callback_t)(bool connected, const String& status);
    void setStatusCallback(nwc_status_callback_t callback);
    
    // UI element references
    void setRelayStatusLabel(lv_obj_t* label);
    
    // Access to internal components for other modules
    WebSocketsClient& getWebSocketClient();
    String getRelayUrl();
    String getWalletPubKey();
    String getNsecHex();
    String getNpubHex();
    
    // Constants for configuration
    namespace Config {
        const unsigned long WS_PING_INTERVAL = 5000; // 5 seconds
        const unsigned long WS_FRAGMENT_TIMEOUT = 30000; // 30 seconds
        const size_t WS_MAX_FRAGMENT_SIZE = 1024 * 1024; // 1MB
        const unsigned long PRICE_UPDATE_INTERVAL = 300000; // 5 minutes
        const int MAX_CURRENCIES = 8; // USD, EUR, GBP, CAD, CHF, AUD, JPY
    }
    
    // Currency indices for price array
    namespace Currency {
        const int USD = 0;
        const int EUR = 1;
        const int GBP = 2;
        const int CAD = 3;
        const int CHF = 4;
        const int AUD = 5;
        const int JPY = 6;
        const int CNY = 7;
    }
}