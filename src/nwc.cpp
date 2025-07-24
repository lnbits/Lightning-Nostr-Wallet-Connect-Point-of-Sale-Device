#include <Arduino.h>
#include "nwc.h"
#include "settings.h"
#include "ui.h"
#include "app.h"
#include <Preferences.h>

namespace NWC {
    // WebSocket client
    static WebSocketsClient webSocket;
    static unsigned long last_loop_time = 0;
    
    // NWC configuration
    static String nwcPairingUrl = "nostr+walletconnect://dbb2f09400c14e80a2d7926e634221ddf0d5f5144aada66a996bfd2e88ca7cde?relay=wss://relay.nostriot.com&secret=e2b73178e2119612cad6457530bee54ed10067b249ab3dbfbbf5a3c770da3708";
    static String relayUrl;
    static String relayPath = "/";
    static String walletPubKey;
    static String nsecHexStr;
    static String npubHexStr;
    
    // Connection state
    static bool nwc_initialized = false;
    static bool connection_in_progress = false;
    static unsigned long last_connection_attempt = 0;
    static unsigned long last_ws_ping = 0;
    static unsigned long last_ws_message_received = 0;
    static unsigned long last_connection_health_check = 0;
    static int reconnection_attempts = 0;
    static const unsigned long MIN_RECONNECT_INTERVAL = 5000; // 5 seconds
    static const unsigned long CONNECTION_TIMEOUT = 30000; // 30 seconds without response
    static const unsigned long HEALTH_CHECK_INTERVAL = 10000; // 10 seconds
    static const unsigned long PING_INTERVAL = 15000; // 15 seconds
    static const int MAX_RECONNECT_ATTEMPTS = 10;
    
    // WebSocket fragment management
    static bool ws_fragment_in_progress = false;
    static String ws_fragmented_message = "";
    static size_t ws_fragment_total_size = 0;
    static size_t ws_fragment_received_size = 0;
    static bool ws_fragment_is_binary = false;
    static unsigned long ws_fragment_start_time = 0;
    
    // NTP time synchronization
    static WiFiUDP ntpUDP;
    static NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);
    static unsigned long unixTimestamp = 0;
    static unsigned long bootTimestamp = 0;
    
    // Bitcoin price data
    static long btc_prices[Config::MAX_CURRENCIES] = {0};
    static bool prices_loaded = false;
    static unsigned long last_price_update = 0;
    
    // Invoice management
    static String current_invoice_str = "";
    static lv_timer_t* invoice_notification_watchdog_timer = NULL;
    static lv_timer_t* invoice_lookup_timer = NULL;
    
    // UI element references
    static lv_obj_t* relay_status_label = NULL;
    
    // Status callback
    static nwc_status_callback_t status_callback = nullptr;
    
    // Preferences instance
    static Preferences preferences;
    
    void init() {
        // Initialize Nostr memory space
        nostr::initMemorySpace(2048, 2048);
        
        timeClient.begin();
        timeClient.setTimeOffset(0);
        bootTimestamp = millis();
        
        // Initialize connection tracking variables
        last_ws_message_received = millis();
        last_connection_health_check = millis();
        reconnection_attempts = 0;
        
        // Load NWC URL from preferences
        loadUrlFromPreferences();
        
        // Setup NWC data if URL is available
        if (nwcPairingUrl.length() > 0) {
            setupData(nwcPairingUrl);
        }
    }
    
    void cleanup() {
        if (invoice_notification_watchdog_timer) {
            lv_timer_del(invoice_notification_watchdog_timer);
            invoice_notification_watchdog_timer = NULL;
        }
        
        if (invoice_lookup_timer) {
            lv_timer_del(invoice_lookup_timer);
            invoice_lookup_timer = NULL;
        }
        
        webSocket.disconnect();
        nwc_initialized = false;
    }
    
    void setupData(const String& url) {
        nwcPairingUrl = url;
        
        // Parse the NWC URL to extract relay and secret
        if (url.startsWith("nostr+walletconnect://")) {
            String urlPart = url.substring(22); // Remove "nostr+walletconnect://"
            
            int queryIndex = urlPart.indexOf('?');
            if (queryIndex != -1) {
                walletPubKey = urlPart.substring(0, queryIndex);
                String queryParams = urlPart.substring(queryIndex + 1);
                
                // Parse query parameters
                int relayIndex = queryParams.indexOf("relay=");
                int secretIndex = queryParams.indexOf("secret=");
                
                if (relayIndex != -1) {
                    int relayEnd = queryParams.indexOf('&', relayIndex);
                    if (relayEnd == -1) relayEnd = queryParams.length();
                    relayUrl = queryParams.substring(relayIndex + 6, relayEnd);
                    // URL decode
                    relayUrl.replace("%3A", ":");
                    relayUrl.replace("%2F", "/");
                }
                
                if (secretIndex != -1) {
                    int secretEnd = queryParams.indexOf('&', secretIndex);
                    if (secretEnd == -1) secretEnd = queryParams.length();
                    nsecHexStr = queryParams.substring(secretIndex + 7, secretEnd);
                }
            }
        }
        
        Serial.println("NWC Data Setup:");
        Serial.println("Wallet PubKey: " + walletPubKey);
        Serial.println("Relay URL: " + relayUrl);
        Serial.println("Secret: " + nsecHexStr);
        
        // Generate npub from nsec if needed
        if (nsecHexStr.length() > 0) {
            int byteSize = 32;
            byte privateKeyBytes[byteSize];
            fromHex(nsecHexStr.c_str(), privateKeyBytes, byteSize);

            PrivateKey privateKey(privateKeyBytes);
            PublicKey pub = privateKey.publicKey();
    
            // Extract only the X coordinate (32 bytes) for Nostr
            byte pubKeyBytes[33];
            pub.serialize(pubKeyBytes, sizeof(pubKeyBytes));
            
            // Convert the X coordinate (bytes 1-32) to hex string
            char npubHex[65];
            for(int i = 1; i < 33; i++) {
                sprintf(npubHex + (i-1)*2, "%02x", pubKeyBytes[i]);
            }
            npubHex[64] = '\0';
            npubHexStr = String(npubHex);
            saveUrlToPreferences(url);
        }
    }
    
    void connectToRelay() {
        if (relayUrl.length() == 0) {
            Serial.println("No relay URL configured");
            return;
        }
        
        // Prevent multiple simultaneous connection attempts
        unsigned long current_time = millis();
        if (connection_in_progress) {
            Serial.println("Connection already in progress, skipping");
            return;
        }
        
        // Rate limit connection attempts
        if (current_time - last_connection_attempt < MIN_RECONNECT_INTERVAL) {
            Serial.println("Too soon to reconnect, waiting...");
            return;
        }
        
        // Check if already connected
        if (webSocket.isConnected()) {
            Serial.println("WebSocket already connected");
            return;
        }
        
        connection_in_progress = true;
        last_connection_attempt = current_time;
        
        // Parse relay URL
        String host;
        int port = 443; // Default WSS port
        String path = "/";
        
        if (relayUrl.startsWith("wss://")) {
            host = relayUrl.substring(6);
            int pathIndex = host.indexOf('/');
            if (pathIndex != -1) {
                path = host.substring(pathIndex);
                host = host.substring(0, pathIndex);
            }
            
            int portIndex = host.indexOf(':');
            if (portIndex != -1) {
                port = host.substring(portIndex + 1).toInt();
                host = host.substring(0, portIndex);
            }
        }
        
        Serial.printf("Connecting to relay: %s:%d%s\\n", host.c_str(), port, path.c_str());
        
        webSocket.beginSSL(host, port, path, "", "Sec-WebSocket-Protocol: echo-protocol");
        webSocket.onEvent([](WStype_t type, uint8_t* payload, size_t length) {
            websocketEvent(type, payload, length);
        });
        webSocket.setReconnectInterval(5000);
        webSocket.enableHeartbeat(15000, 3000, 2);
        
        nwc_initialized = true;
        
        Serial.println("WebSocket connection initiated");
    }
    
    void restartConnection() {
        Serial.println("Restarting NWC connection...");
        webSocket.disconnect();
        nwc_initialized = false;
        connection_in_progress = false;  // Reset connection state
        
        if (relayUrl.length() > 0) {
            delay(1000);
            connectToRelay();
        }
    }
    
    void disconnect() {
        webSocket.disconnect();
        nwc_initialized = false;
        connection_in_progress = false;  // Reset connection state
        resetWebsocketFragmentState();
        
        if (status_callback) {
            status_callback(false, "Disconnected");
        }
    }
    
    bool isInitialized() {
        return nwc_initialized;
    }
    
    bool isConnected() {
        return webSocket.isConnected();
    }
    
    void loadUrlFromPreferences() {
        preferences.begin("nwc-config", true);
        String saved_url = preferences.getString("nwc_url", "");
        preferences.end();
        
        if (saved_url.length() > 0) {
            nwcPairingUrl = saved_url;
            Serial.println("Loaded NWC URL from preferences: " + nwcPairingUrl);
        } else {
            Serial.println("No saved NWC URL found, using default");
        }
    }
    
    void saveUrlToPreferences(const String& url) {
        preferences.begin("nwc-config", false);
        preferences.putString("nwc_url", url);
        preferences.end();
        Serial.println("Saved NWC URL to preferences: " + url);
    }
    
    String getPairingUrl() {
        return nwcPairingUrl;
    }
    
    void setPairingUrl(const String& url) {
        nwcPairingUrl = url;
        saveUrlToPreferences(url);
        setupData(url);
    }
    
    void websocketEvent(WStype_t type, uint8_t* payload, size_t length) {
        switch (type) {
            case WStype_DISCONNECTED:
                Serial.printf("[WSc] Disconnected!\\n");
                nwc_initialized = false;
                connection_in_progress = false;
                resetWebsocketFragmentState();
                updateRelayStatusDisplay(false);
                if (status_callback) {
                    status_callback(false, "Disconnected");
                }
                break;
                
            case WStype_CONNECTED:
                Serial.printf("[WSc] Connected to: %s\\n", payload);
                nwc_initialized = true;
                connection_in_progress = false;
                last_ws_ping = millis();
                last_ws_message_received = millis();
                reconnection_attempts = 0; // Reset reconnection counter on successful connection
                updateRelayStatusDisplay(true);
                if (status_callback) {
                    status_callback(true, "Connected");
                }
                
                // Subscribe to events
                {
                    String subscriptionId = "";
                    for (int i = 0; i < 16; i++) {
                        subscriptionId += String(random(16), HEX);
                    }
                    
                    String subscribeMessage = "[\"REQ\",\"" + subscriptionId + "\",{\"kinds\":[23195,23196,23197],\"#p\":[\"" + npubHexStr + "\"]}]";
                    webSocket.sendTXT(subscribeMessage);
                    Serial.println("Sent subscription: " + subscribeMessage);
                }
                break;
                
            case WStype_TEXT:
                Serial.printf("[WSc] Received text: %s\\n", payload);
                last_ws_message_received = millis(); // Track message reception
                handleWebsocketMessage(NULL, payload, length);
                break;
                
            case WStype_BIN:
                last_ws_message_received = millis(); // Track binary message reception
                Serial.printf("[WSc] Received binary length: %u\\n", length);
                break;
                
            case WStype_FRAGMENT_TEXT_START:
                Serial.printf("[WSc] Fragment text start, length: %u\\n", length);
                ws_fragment_in_progress = true;
                ws_fragment_is_binary = false;
                ws_fragmented_message = String((char*)payload);
                ws_fragment_start_time = millis();
                ws_fragment_received_size = length;
                break;
                
            case WStype_FRAGMENT_BIN_START:
                Serial.printf("[WSc] Fragment binary start, length: %u\\n", length);
                ws_fragment_in_progress = true;
                ws_fragment_is_binary = true;
                ws_fragment_start_time = millis();
                ws_fragment_received_size = length;
                break;
                
            case WStype_FRAGMENT:
                if (ws_fragment_in_progress) {
                    ws_fragment_received_size += length;
                    if (!ws_fragment_is_binary) {
                        ws_fragmented_message += String((char*)payload);
                    }
                    Serial.printf("[WSc] Fragment received, total size: %u\\n", ws_fragment_received_size);
                }
                break;
                
            case WStype_FRAGMENT_FIN:
                if (ws_fragment_in_progress) {
                    ws_fragment_received_size += length;
                    if (!ws_fragment_is_binary) {
                        ws_fragmented_message += String((char*)payload);
                        Serial.printf("[WSc] Fragment finished, final message: %s\\n", ws_fragmented_message.c_str());
                        handleWebsocketMessage(NULL, (uint8_t*)ws_fragmented_message.c_str(), ws_fragmented_message.length());
                    }
                    resetWebsocketFragmentState();
                }
                break;
                
            case WStype_ERROR:
                Serial.printf("[WSc] Error: %s\\n", payload);
                break;
                
            case WStype_PONG:
                Serial.printf("[WSc] Pong received\\n");
                break;
                
            default:
                break;
        }
    }
    
    void handleWebsocketMessage(void* arg, uint8_t* data, size_t len) {
        String message = String((char*)data);
        Serial.println("Processing WebSocket message: " + message);
        
        // Parse JSON message
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, message);
        
        if (error) {
            Serial.println("Failed to parse JSON message");
            return;
        }
        
        // Check if it's an event array
        if (doc.is<JsonArray>() && doc.size() >= 3) {
            String messageType = doc[0];
            if (messageType == "EVENT") {
                JsonObject event = doc[2];
                int kind = event["kind"];
                
                switch (kind) {
                    case 23195:
                        handleNwcResponseEvent(data);
                        break;
                    case 23196:
                        handleNwcNip04NotificationEvent(data);
                        break;
                    case 23197:
                        handleNwcNip44NotificationEvent(data);
                        break;
                    default:
                        Serial.println("Received event with unknown kind: " + String(kind));
                        break;
                }
            }
        }
    }
    
    void resetWebsocketFragmentState() {
        ws_fragment_in_progress = false;
        ws_fragmented_message = "";
        ws_fragment_total_size = 0;
        ws_fragment_received_size = 0;
        ws_fragment_is_binary = false;
        ws_fragment_start_time = 0;
    }
    
    void handleNwcResponseEvent(uint8_t* data) {
        Serial.println("Handling NWC response event (kind 23195)");
        
        String message = String((char*)data);
        
        // Parse the event message
        DynamicJsonDocument eventDoc(2048);
        DeserializationError error = deserializeJson(eventDoc, message);
        
        if (error) {
            Serial.print("JSON parsing failed: ");
            Serial.println(error.c_str());
            return;
        }
        
        // Extract event content
        JsonObject event = eventDoc[2];
        String content = event["content"];
        String thirdPartyPubKey = event["pubkey"];
        
        Serial.println("Third Party PubKey: " + thirdPartyPubKey);
        Serial.println("Cipher Text: " + content);
        
        if (content.length() == 0 || thirdPartyPubKey.length() == 0) {
            Serial.println("Failed to extract content or pubkey from event");
            return;
        }
        
        // Decrypt the response
        String decryptedResponse = nostr::decryptNip04Ciphertext(content, nsecHexStr.c_str(), thirdPartyPubKey);
        Serial.println("Decrypted Response: " + decryptedResponse);
        
        // Parse the decrypted response
        DynamicJsonDocument decryptedResponseDoc(1024);
        deserializeJson(decryptedResponseDoc, decryptedResponse);
        String resultType = decryptedResponseDoc["result_type"];
        Serial.println("Result Type: " + resultType);
        
        if (resultType == "make_invoice") {
            Serial.println("=== Processing make_invoice result ===");
            Serial.println("Free heap before processing: " + String(ESP.getFreeHeap()));
            
            JsonObject result = decryptedResponseDoc["result"];
            
            // Check if result object is valid
            if (result.isNull()) {
                Serial.println("ERROR: Result object is null!");
                return;
            }
            
            String invoice = result["invoice"].as<String>();
            String description = result["description"];
            int amount = result["amount"];
            
            Serial.println("Invoice: " + invoice);
            Serial.println("Description: " + description);
            Serial.println("Amount: " + String(amount) + " msats");
            Serial.println("Invoice validity check: " + String(invoice.length() > 0 ? "VALID" : "INVALID"));
            
            if (invoice.length() == 0) {
                Serial.println("ERROR: Empty invoice received!");
                return;
            }
            
            // Store the current invoice
            Serial.println("Storing current invoice...");
            setCurrentInvoice(invoice);
            Serial.println("Invoice stored successfully");
            
            Serial.println("Free heap before UI update: " + String(ESP.getFreeHeap()));
            
            // Notify the App module that invoice was created
            // App::notifyInvoiceCreated(invoice) will be called externally
            Serial.println("Calling UI::updateInvoiceDisplay...");
            UI::updateInvoiceDisplay(invoice, amount / 1000); // Convert msats to sats
            Serial.println("UI::updateInvoiceDisplay completed");
            
            Serial.println("Free heap after UI update: " + String(ESP.getFreeHeap()));
            
            // Start watching for payment notifications
            Serial.println("Starting invoice notification watchdog...");
            startInvoiceNotificationWatchdog();
            
            // Start invoice lookup timer (after 3 seconds)
            Serial.println("Starting invoice lookup timer...");
            startInvoiceLookupTimer();
            Serial.println("=== make_invoice processing completed ===");
        } else if (resultType == "lookup_invoice") {
            Serial.println("Received lookup_invoice response:");
            String resultJson;
            serializeJson(decryptedResponseDoc["result"], resultJson);
            Serial.println(resultJson);

            Serial.println("Settled at value is: " + decryptedResponseDoc["result"]["settled_at"].as<String>());
            
            // check if settled at is a timestamp
            if (decryptedResponseDoc["result"].containsKey("settled_at") && decryptedResponseDoc["result"]["settled_at"] >= 1231006505) {
                Serial.println("Invoice is settled (paid).");
                
                // Stop the lookup timer
                stopInvoiceLookupTimer();
                
                // Stop the notification watchdog
                stopInvoiceNotificationWatchdog();
                
                // Notify payment received
                Serial.println("Payment received - updating UI");
                UI::showPaymentReceived();
                
                // Clear the current invoice
                setCurrentInvoice("");
                
            } else {
                Serial.println("Invoice is not settled yet.");
            }
        }
    }
    
    void handleNwcNip04NotificationEvent(uint8_t* data) {
        Serial.println("Handling NWC NIP-04 notification event (kind 23196)");
        // Implementation would go here
        // This would handle NIP-04 encrypted notifications
    }
    
    void handleNwcNip44NotificationEvent(uint8_t* data) {
        Serial.println("Handling NWC NIP-44 notification event (kind 23197)");
        // Implementation would go here
        // This would handle NIP-44 encrypted notifications
    }
    
    void makeInvoice(float amount, const String& currency) {
        if (!isConnected()) {
            Serial.println("WebSocket not connected, cannot create invoice");
            return;
        }
        
        // Calculate sats based on entered amount and selected currency
        int amount_sats;
        if (currency == "sats") {
            amount_sats = (int)amount;
        } else {
            amount_sats = calculateSatsFromAmount(amount, currency);
        }
        
        // Create the make_invoice request
        DynamicJsonDocument doc(512);
        doc["method"] = "make_invoice";
        JsonObject params = doc.createNestedObject("params");
        params["amount"] = amount_sats * 1000; // Convert to msats
        params["description"] = "Payment to " + Settings::getShopName();
        
        String requestJson;
        serializeJson(doc, requestJson);
        
        Serial.println("Sending make_invoice request: " + requestJson);
        Serial.println("Amount: " + String(amount) + " " + currency + " = " + String(amount_sats) + " sats");
        
        // Encrypt and send the request
        String encryptedDm = nostr::getEncryptedDm(nsecHexStr.c_str(), npubHexStr.c_str(), walletPubKey.c_str(), 23194, getUnixTimestamp(), requestJson, "nip04");
        webSocket.sendTXT(encryptedDm);
        
        doc.clear();
    }
    
    void sendInvoiceLookupRequest() {
        if (!isConnected()) {
            Serial.println("WebSocket not connected, cannot send invoice lookup");
            return;
        }
        
        if (current_invoice_str.isEmpty()) {
            Serial.println("No current invoice to lookup");
            return;
        }
        
        Serial.println("Sending invoice lookup request for: " + current_invoice_str);
        
        // Create the lookup_invoice request
        DynamicJsonDocument doc(512);
        doc["method"] = "lookup_invoice";
        JsonObject params = doc.createNestedObject("params");
        params["invoice"] = current_invoice_str;
        
        String requestJson;
        serializeJson(doc, requestJson);
        
        Serial.println("Sending lookup_invoice request: " + requestJson);
        
        // Encrypt and send the request
        String encryptedDm = nostr::getEncryptedDm(nsecHexStr.c_str(), npubHexStr.c_str(), walletPubKey.c_str(), 23194, getUnixTimestamp(), requestJson, "nip04");
        webSocket.sendTXT(encryptedDm);
        
        doc.clear();
    }
    
    void startInvoiceNotificationWatchdog() {
        if (invoice_notification_watchdog_timer == NULL) {
            invoice_notification_watchdog_timer = lv_timer_create(invoiceNotificationWatchdogCallback, 30000, NULL);
        }
    }
    
    void stopInvoiceNotificationWatchdog() {
        if (invoice_notification_watchdog_timer) {
            lv_timer_del(invoice_notification_watchdog_timer);
            invoice_notification_watchdog_timer = NULL;
        }
    }
    
    void startInvoiceLookupTimer() {
        if (invoice_lookup_timer == NULL) {
            invoice_lookup_timer = lv_timer_create(invoiceLookupTimerCallback, 3000, NULL); // 3 seconds
        }
    }
    
    void stopInvoiceLookupTimer() {
        if (invoice_lookup_timer) {
            lv_timer_del(invoice_lookup_timer);
            invoice_lookup_timer = NULL;
        }
    }
    
    String getCurrentInvoice() {
        return current_invoice_str;
    }
    
    void setCurrentInvoice(const String& invoice) {
        current_invoice_str = invoice;
    }
    
    void fetchBitcoinPrices() {
        if (!WiFi.isConnected()) {
            Serial.println("WiFi not connected, cannot fetch prices");
            return;
        }
        
        HTTPClient http;
        http.begin("https://mempool.space/api/v1/prices");
        int httpResponseCode = http.GET();
        
        if (httpResponseCode == 200) {
            String response = http.getString();
            updatePricesFromJson(response);
            last_price_update = millis();
            prices_loaded = true;
            Serial.println("Bitcoin prices updated successfully");
        } else {
            Serial.printf("HTTP request failed with code: %d\\n", httpResponseCode);
        }
        
        http.end();
    }
    
    void updatePricesFromJson(const String& jsonResponse) {
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, jsonResponse);
        
        if (error) {
            Serial.println("Failed to parse price JSON");
            return;
        }
        
        // Update price array
        btc_prices[Currency::USD] = doc["USD"].as<long>();
        btc_prices[Currency::EUR] = doc["EUR"].as<long>();
        btc_prices[Currency::GBP] = doc["GBP"].as<long>();
        btc_prices[Currency::CAD] = doc["CAD"].as<long>();
        btc_prices[Currency::CHF] = doc["CHF"].as<long>();
        btc_prices[Currency::AUD] = doc["AUD"].as<long>();
        btc_prices[Currency::JPY] = doc["JPY"].as<long>();
        btc_prices[Currency::CNY] = doc["CNY"].as<long>();
        
        Serial.printf("Updated prices - USD: %ld, EUR: %ld, GBP: %ld\\n", 
                     btc_prices[Currency::USD], btc_prices[Currency::EUR], btc_prices[Currency::GBP]);
    }
    
    int calculateSatsFromAmount(float amount, const String& currency) {
        if (!prices_loaded) {
            return 0;
        }
        
        long price = 0;
        if (currency == "USD") price = btc_prices[Currency::USD];
        else if (currency == "EUR") price = btc_prices[Currency::EUR];
        else if (currency == "GBP") price = btc_prices[Currency::GBP];
        else if (currency == "CAD") price = btc_prices[Currency::CAD];
        else if (currency == "CHF") price = btc_prices[Currency::CHF];
        else if (currency == "AUD") price = btc_prices[Currency::AUD];
        else if (currency == "JPY") price = btc_prices[Currency::JPY];
        else if (currency == "CNY") price = btc_prices[Currency::CNY];
        
        if (price > 0) {
            return (int)((amount / price) * 100000000); // Convert to satoshis
        }
        
        return 0;
    }
    
    long* getBitcoinPrices() {
        return btc_prices;
    }
    
    bool arePricesLoaded() {
        return prices_loaded;
    }
    
    unsigned long getLastPriceUpdate() {
        return last_price_update;
    }
    
    void attemptReconnectionIfNeeded() {
        unsigned long current_time = millis();
        
        // Only proceed if WiFi is connected
        if (WiFi.status() != WL_CONNECTED) {
            if (isConnected() || nwc_initialized) {
                Serial.println("WiFi disconnected, resetting NWC connection state");
                nwc_initialized = false;
                updateRelayStatusDisplay(false);
            }
            return;
        }
        
        // Check connection health periodically
        if (current_time - last_connection_health_check >= HEALTH_CHECK_INTERVAL) {
            last_connection_health_check = current_time;
            
            // If we think we're connected but haven't received any messages for too long, force reconnect
            if (isConnected() && nwc_initialized) {
                if (current_time - last_ws_message_received > CONNECTION_TIMEOUT) {
                    Serial.println("Connection appears stale (no messages for " + String(CONNECTION_TIMEOUT/1000) + "s), forcing disconnect");
                    webSocket.disconnect();
                    nwc_initialized = false;
                    updateRelayStatusDisplay(false);
                    return;
                }
                
                // Send periodic ping to keep connection alive
                if (current_time - last_ws_ping > PING_INTERVAL) {
                    sendPing();
                }
            }
        }
        
        // Attempt reconnection if not connected and not already trying
        if (!isConnected() && !connection_in_progress) {
            // Calculate backoff interval based on number of attempts
            unsigned long backoff_interval = MIN_RECONNECT_INTERVAL * (1 << min(reconnection_attempts, 5)); // Exponential backoff, max 32x
            
            if (current_time - last_connection_attempt >= backoff_interval) {
                if (reconnection_attempts < MAX_RECONNECT_ATTEMPTS) {
                    Serial.println("Attempting WebSocket reconnection (attempt " + String(reconnection_attempts + 1) + "/" + String(MAX_RECONNECT_ATTEMPTS) + ")");
                    reconnection_attempts++;
                    connectToRelay();
                } else {
                    // Reset reconnection attempts after max attempts reached, but wait longer
                    if (current_time - last_connection_attempt >= (backoff_interval * 4)) {
                        Serial.println("Resetting reconnection attempts after extended wait");
                        reconnection_attempts = 0;
                    }
                }
            }
        }
    }
    
    void updateRelayStatusDisplay(bool connected) {
        if (status_callback) {
            status_callback(connected, connected ? "Relay Connected" : "Relay Disconnected");
        }
        
        // Update the relay status label
        if (relay_status_label != NULL && lv_obj_is_valid(relay_status_label)) {
            if (connected) {
                String relay_status_text = LV_SYMBOL_OK + String(" Relay: Connected");
                lv_label_set_text(relay_status_label, relay_status_text.c_str());
                lv_obj_set_style_text_color(relay_status_label, lv_color_hex(0x00FF00), 0);
            } else {
                String relay_status_text = "Relay: Disconnected";
                lv_label_set_text(relay_status_label, relay_status_text.c_str());
                lv_obj_set_style_text_color(relay_status_label, lv_color_hex(0x9E9E9E), 0);
            }
        }
    }
    
    void updateTime() {
        // Only update time from NTP server if WiFi is connected
        // This prevents DNS resolution failures when there's no network
        if (WiFi.status() == WL_CONNECTED) {
            timeClient.update();
        }
        unixTimestamp = timeClient.getEpochTime();
    }
    
    unsigned long getUnixTimestamp() {
        return unixTimestamp;
    }
    
    unsigned long getBootTimestamp() {
        return bootTimestamp;
    }
    
    NTPClient& getTimeClient() {
        return timeClient;
    }
    
    void sendPing() {
        if (isConnected()) {
            webSocket.sendPing();
            last_ws_ping = millis();
        }
    }
    
    void processLoop() {
        // Only process WebSocket events if WiFi is connected
        // This prevents the WebSocket from blocking the main loop when there's no network
        if (WiFi.status() == WL_CONNECTED) {
            static unsigned long last_loop_time = 0;
            if (millis() - last_loop_time > 3000) {
                Serial.println("=== NWC loop ===");
                Serial.println("Free heap: " + String(ESP.getFreeHeap()));
                last_loop_time = millis();
            }
            webSocket.loop();
            
            // Check for fragment timeout
            if (ws_fragment_in_progress && (millis() - ws_fragment_start_time > Config::WS_FRAGMENT_TIMEOUT)) {
                Serial.println("Fragment timeout, resetting state");
                resetWebsocketFragmentState();
            }
            
            // Ping handling is now done in attemptReconnectionIfNeeded()
        } else {
            // WiFi not connected - reset connection state to prevent hanging
            if (nwc_initialized && !connection_in_progress) {
                Serial.println("WiFi disconnected, resetting NWC connection state");
                nwc_initialized = false;
                connection_in_progress = false;
                resetWebsocketFragmentState();
            }
        }
    }
    
    bool isFragmentInProgress() {
        return ws_fragment_in_progress;
    }
    
    void handleFragment(uint8_t* payload, size_t length, bool isBinary, bool isLast) {
        ws_fragment_received_size += length;
        
        if (!isBinary) {
            ws_fragmented_message += String((char*)payload);
        }
        
        if (isLast) {
            if (!isBinary) {
                handleWebsocketMessage(NULL, (uint8_t*)ws_fragmented_message.c_str(), ws_fragmented_message.length());
            }
            resetWebsocketFragmentState();
        }
    }
    
    void invoiceLookupTimerCallback(lv_timer_t* timer) {
        // Check if invoice processing is still active
        if (!UI::isInvoiceProcessing()) {
            Serial.println("Invoice lookup cancelled - overlay not active");
            stopInvoiceLookupTimer();
            return;
        }
        
        // Don't send lookup requests during light sleep (unless invoice is active)
        if (App::isInLightSleep()) {
            Serial.println("Invoice lookup skipped - device in light sleep");
            return;
        }
        
        sendInvoiceLookupRequest();
    }
    
    void invoiceNotificationWatchdogCallback(lv_timer_t* timer) {
        Serial.println("Invoice notification watchdog timer fired");
        // Implementation would handle invoice timeout
    }
    
    void setStatusCallback(nwc_status_callback_t callback) {
        status_callback = callback;
    }
    
    void setRelayStatusLabel(lv_obj_t* label) {
        relay_status_label = label;
        // Update display with current connection state
        updateRelayStatusDisplay(isConnected());
    }
    
    WebSocketsClient& getWebSocketClient() {
        return webSocket;
    }
    
    String getRelayUrl() {
        return relayUrl;
    }
    
    String getWalletPubKey() {
        return walletPubKey;
    }
    
    String getNsecHex() {
        return nsecHexStr;
    }
    
    String getNpubHex() {
        return npubHexStr;
    }
}