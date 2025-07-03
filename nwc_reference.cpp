#include <Arduino.h>
#include "Bitcoin.h"
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TFT_eSPI.h>

#include "nostr.h"
#include <NostrEvent.h>

#define TFT_MOSI            19
#define TFT_SCLK            18
#define TFT_CS              5
#define TFT_DC              16
#define TFT_RST             23
TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library


#define TFT_BL          4   // Display backlight control pin
#define ADC_EN          14  //ADC_EN is the ADC detection enable port
#define ADC_PIN         34


#define RXD2 13  // ESP32 receives from GM803 TX
#define TXD2 17  // ESP32 sends to GM803 RX
#define MAX_QR_LENGTH 1000  // Maximum expected QR code length

WebSocketsClient webSocket;

String ssid = "wubwub";
String password = "blob19750405blob";

String nwcPairingUrl = "nostr+walletconnect://dbb2f09400c14e80a2d7926e634221ddf0d5f5144aada66a996bfd2e88ca7cde?relay=wss://relay.nostriot.com&secret=20f2a32b6f0d106b4de90d32647d68f5b301da9f251b7f1612cafff4c17153ca";
const char *nsecHex;
const char *npubHex;
String relayUrl;
String walletPubKey;
unsigned long lastScanTime = 0; // Track when we last processed a QR code
const unsigned long DEBOUNCE_TIME = 5000; // Minimum time between scans in milliseconds

unsigned long unixTimestamp;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "uk.pool.ntp.org", 0, 60000);

const char *serialisedJson;
DynamicJsonDocument eventDoc(0);
DynamicJsonDocument eventParamsDoc(0);

struct DocumentData
{
  String tags;
  uint16_t kind;
  String content;
  unsigned long timestamp;

  DocumentData(String t, uint16_t k, String c, unsigned long ts) : tags(t), kind(k), content(c), timestamp(ts) {}
};

char qrBuffer[MAX_QR_LENGTH];
int bufferIndex = 0;
unsigned long lastCharTime = 0;
const unsigned long TIMEOUT = 50; // Timeout between characters in milliseconds

void showMessage(String title, String message)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN,TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.drawString(title, 0, 0);
  tft.drawString(message, 0, 20);
}

void getRandom64ByteHex(char *output)
{
  for (int i = 0; i < 64; i++)
  {
    output[i] = "0123456789abcdef"[esp_random() % 16];
  }
  output[64] = '\0';
}

uint8_t socketDisconnectCount = 0;

// connect to web socket server. set up callbacks, on connect, on disconnect, on message
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    ++socketDisconnectCount;
    if (socketDisconnectCount > 3)
    {
      showMessage("Error", "Failed to connect to relay.");
      ESP.restart();
    }
    Serial.printf("[WSc] Disconnected!\n");
    showMessage("Disconnected from relay", "Reconnecting..");
    webSocket.beginSSL(relayUrl, 443);
    webSocket.onEvent(webSocketEvent);
    break;
  case WStype_CONNECTED:
  {
    socketDisconnectCount = 0;
    Serial.printf("[WSc] Connected to %s\n", relayUrl);
    showMessage("Connected to Nostr", relayUrl);
    delay(100);
    showMessage("Ready to pay", "Scan a lightning invoice");
    // watch for NWC events
    String npubHexString(npubHex);
    char *subscriptionId = new char[64];
    getRandom64ByteHex(subscriptionId);

    String req = "[\"REQ\", \"" + String(subscriptionId) + "\",{\"kinds\":[23195],\"#p\":[\"" + npubHexString + "\"],\"limit\":0}]";
    Serial.println("req is: " + req);
    webSocket.sendTXT(req);
    break;
  }
  case WStype_TEXT:
  {
    Serial.printf("[WSc] Got WS TEXT\n");
    Serial.println("Payload is: " + String((char *)payload));
    handleWebSocketMessage(NULL, payload, length);
    break;
  }
  case WStype_BIN:
    Serial.printf("[WSc] get binary length: %u\n", length);
    break;
  }
}

void handleNwcResponseEvent(uint8_t *data)
{
  Serial.println("NWC Response Event");
  Serial.println(String((char *)data));

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, (char *)data);
  String eventContent = doc[2];
  Serial.println("Event Content: " + eventContent);
  doc.clear();

  // eventContent looks like this: {"content":"0HAsApvCB3Fl40PnlzNsdHLqMgog+IST+c5uzqIKJXgyXktpGI0LrK636FB6qXC1h4mWzWpDB2SgoQ3ybcDcJlR3lMKoaON3WXTokX7FVsaTTNvgxS6DZp+QOl3Nq3VDNlYgHcuDILLZBI5vKRo4IiXgz3Sndo50q/AYGYJrNPA=?iv=yQvhFMDgTi/EEN83jYutAQ==","created_at":1749501280,"id":"85823f790f4bb01e4162607b67f3af3bd8d46de1b707b8b65b3024da3548db11","kind":23195,"pubkey":"dbb2f09400c14e80a2d7926e634221ddf0d5f5144aada66a996bfd2e88ca7cde","sig":"af0ea8aa781b5d51c31d59bb2a7402658cf6961e3473928033711d1690c72fd00edadb811453bb984a05b0de4334898d06ae605decef703a3e20bb4717879c30","tags":[["e","440d51005a6ed98f58c78bb5be70a961ad7ae0e1832437812e3e8633d2e313c3"],["p","d0bfc94bd4324f7df2a7601c4177209828047c4d3904d64009a3c67fb5d5e7ca"]]} 
  // serialise the eventContent into a json object and get the content (ciphertext) and pubkey
  DynamicJsonDocument eventContentDoc(1024);
  deserializeJson(eventContentDoc, eventContent);
  String cipherText = eventContentDoc["content"];
  String thirdPartyPubKey = eventContentDoc["pubkey"];
  Serial.println("Third Party PubKey: " + thirdPartyPubKey);
  Serial.println("Cipher Text: " + cipherText);


  String decryptedResponse = nostr::decryptNip04Ciphertext(cipherText, nsecHex, thirdPartyPubKey);
  Serial.println("Decrypted Response: " + decryptedResponse);
  // dectyptedResponse looks like this: {"result_type":"pay_invoice","result":{"preimage":"0000000000000000000000000000000000000000000000000000000000000000"}}
  // json serialise and get result_type
  DynamicJsonDocument decryptedResponseDoc(1024);
  deserializeJson(decryptedResponseDoc, decryptedResponse);
  String resultType = decryptedResponseDoc["result_type"];
  Serial.println("Result Type: " + resultType);
  if(resultType == "pay_invoice") {
    showMessage("Payment Successful", "Invoice paid successfully.");
  } else {
    showMessage("Payment Failed", "result_type: " + resultType);
  }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  if (strstr((char *)data, "EVENT") && strstr((char *)data, "23195"))
  {
    handleNwcResponseEvent(data);
  }
}

void setup() {
  Serial.begin(115200);  // USB debug
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  // TFT
  tft.init();
  tft.setRotation(1);
  showMessage("NWC", "Initialising...");

  delay(100);
  setupNwcData(nwcPairingUrl);

  // connect to wifi
  Serial.println("Connecting to WiFi...");
  showMessage("Connecting to WiFi", String(ssid));

  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);

  unsigned long startWifiConnectionAttempt = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(300);
    Serial.print(".");
    if (millis() - startWifiConnectionAttempt > 5000)
    {
      showMessage("Error", "Failed to connect to WiFi.");
      ESP.restart();
      return;
    }
  }

  nostr::initMemorySpace(1024, 1024);

  timeClient.begin();

  webSocket.beginSSL(relayUrl, 443);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void requestLightningInvoicePayment(String invoice) {
  // now we request the NWC service to pay the invoice with this structure:
    String req = "{\"method\":\"pay_invoice\",\"params\":{\"invoice\":\"" + invoice + "\"}}";
    String encryptedDm = nostr::getEncryptedDm(nsecHex, npubHex, walletPubKey.c_str(), 23194, unixTimestamp, req, "nip04");
    Serial.println("Encrypted DM: " + encryptedDm);
    // now we send the encrypted dm to the NWC service
    webSocket.sendTXT(encryptedDm);
}

void handleLightningInvoice(String invoice) {
  invoice.toLowerCase();
  invoice.replace("lightning:", "");
  if(invoice.startsWith("lnbc")) {
    Serial.println("Lightning Invoice: " + invoice);
    requestLightningInvoicePayment(invoice);
    showMessage("Paying Invoice", "Please wait...");
  } else {
    Serial.println("Not a Lightning Invoice: " + invoice);
    showMessage("Not a Lightning Invoice", invoice);
  }
}

void processQRCode() {
  if (bufferIndex > 0) {
    qrBuffer[bufferIndex] = '\0'; // Null terminate the string
    String qrCode = String(qrBuffer);
    qrCode.trim();
    
    // Debug output
    Serial.print("QR length: ");
    Serial.println(qrCode.length());
    
    // Check if this is a new QR code and enough time has passed
    unsigned long currentTime = millis();
    if ((currentTime - lastScanTime) > DEBOUNCE_TIME) {
      handleLightningInvoice(qrCode);
      lastScanTime = currentTime;
    }
    
    // Reset buffer
    bufferIndex = 0;
  }
}

unsigned long lastPing = 0;

void loop() {
  webSocket.loop();
  // ping the relay every 10 seconds
  if (millis() - lastPing > 10000)
  {
    lastPing = millis();
    webSocket.sendPing();
  }

  timeClient.update();
  unixTimestamp = timeClient.getEpochTime();

  if (Serial2.available()) {
    char c = Serial2.read();
    unsigned long currentTime = millis();
    
    // If we've waited too long since the last character, process the buffer
    if (currentTime - lastCharTime > TIMEOUT && bufferIndex > 0) {
      processQRCode();
    }
    
    // Add character to buffer if we have space
    if (bufferIndex < MAX_QR_LENGTH - 1) {
      qrBuffer[bufferIndex++] = c;
    }
    
    lastCharTime = currentTime;
  } else if (bufferIndex > 0 && millis() - lastCharTime > TIMEOUT) {
    // If no new data and we've waited long enough, process the buffer
    processQRCode();
  }
}

/* a function to get various data from the nwcPairingUrl */
void setupNwcData(String nwcPairingUrl) {
  relayUrl = nwcPairingUrl.substring(nwcPairingUrl.indexOf("relay=") + 6, nwcPairingUrl.indexOf("&secret="));
  relayUrl.replace("wss://", "");
  String nsecHexStr = nwcPairingUrl.substring(nwcPairingUrl.indexOf("secret=") + 7);
  nsecHex = nsecHexStr.c_str();
  walletPubKey = nwcPairingUrl.substring(nwcPairingUrl.indexOf("nostr+walletconnect://") + 22);
  walletPubKey = walletPubKey.substring(0, walletPubKey.indexOf("?"));
  Serial.println("Wallet PubKey: " + walletPubKey);
  Serial.println("Relay URL: " + relayUrl);
  Serial.println("nsecHexStr: " + nsecHexStr);
  Serial.println("nsecHex: " + String(nsecHex));

  // get the public key from the nsecHex
  int byteSize = 32;
  byte privateKeyBytes[byteSize];
  fromHex(nsecHex, privateKeyBytes, byteSize);

  // Create private key object
  PrivateKey privateKey(privateKeyBytes);
  PublicKey pub = privateKey.publicKey();
  String npubHexStr = pub.toString();
  npubHex = npubHexStr.c_str();
  Serial.println("NPub Hex: " + String(npubHex));
}