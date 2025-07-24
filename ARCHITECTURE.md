# NWC Point of Sale Device Architecture

## Project Overview

This is a **Nostr Wallet Connect (NWC) powered Point of Sale device** built for the ESP32-S3 Guition JC3248W535 touch screen display. The device enables merchants to accept Bitcoin Lightning Network payments using the Nostr protocol NIP-47 for wallet communication.

### Key Features
- Nostr Wallet Connect integration for Lightning payments
- Touch screen interface
- WiFi connectivity with AP mode for NWC Pairing Code configuration
- Bitcoin price fetching from multiple fiat currencies
- QR code generation for Lightning invoices
- Real-time payment notifications

### Technology Stack
- **Device**: ESP32-S3 Guition JC3248W535 with 3.5" touch screen display (320x480)
- **GUI Framework**: LVGL (Light and Versatile Graphics Library)
- **Display Driver**: ArduinoGFX
- **Protocols**: Nostr, WebSocket, HTTP/HTTPS, NTP
- **Cryptography**: Bitcoin secp256k1 with Schnorr sigs, AES encryption

---

## Architecture Overview

The application follows a **modular architecture** with clear separation of concerns. The `App` namespace serves as the central coordinator, managing initialization, inter-module communication, and application lifecycle.

```
┌─────────────────┐
│      App        │  ← Central coordinator
│   (app.cpp)     │
└─────┬───────────┘
      │
┌─────┴───────────────────────────────────────┐
│  Module Layer                               │
├─────────┬─────────┬─────────┬─────────┬─────┴──┐
│Display  │Settings │WiFiMgr  │   UI    │  NWC   │
│         │         │         │         │        │
└─────────┴─────────┴─────────┴─────────┴────────┘
```

---

## File Structure and Module Descriptions

### Core Application Files

#### `src/main.cpp`
**Entry point and main loop controller**
- Initializes the entire application via `App::init()`
- Runs the main event loop processing LVGL and application modules
- Manages legacy WiFi connection timeouts and status checking
- Creates task communication queues for modular operation

**Key Functions:**
- `setup()`: System initialization and module startup
- `loop()`: Main application event loop
- `wifi_main_status_updater_cb()`: WiFi connection timeout handling

#### `src/app.cpp` / `src/app.h`
**Central application coordinator and state manager**
- Coordinates initialization and cleanup of all modules
- Manages application state transitions and error handling
- Implements deep sleep power management with GPIO wake-up
- Provides inter-module communication via event callbacks
- Handles system health monitoring and status reporting

**Key Functions:**
- `init()`: Initialize all modules in dependency order
- `run()`: Main application loop with health checks
- `notifyWiFiStatusChanged()`: Coordinate WiFi state changes
- `notifyNWCStatusChanged()`: Handle NWC connection events
- `enterSleepMode()`/`exitSleepMode()`: Power management
- `checkModuleHealth()`: System diagnostics

**Power Management:**
- Configures GPIO 12 as wake-up pin
- Implements 30-second inactivity timeout
- Handles wake-up reason detection and display backlight control

### Display and UI Modules

#### `src/display.cpp` / `src/display.h`
**Hardware display driver and LVGL integration**
- Custom LGFX driver implementation for WT32-SC01
- Configures ST7796 display controller and FT5x06 touch controller
- Provides LVGL display and input device callbacks
- Manages display backlight and power states
- Implements QR code rendering for Lightning invoices

**Key Functions:**
- `init()`: Hardware initialization and LVGL setup
- `displayFlush()`: LVGL display callback
- `touchpadRead()`: LVGL touch input callback
- `displayQRCode()`: Lightning invoice QR code generation
- `turnOffBacklight()`/`turnOnBacklight()`: Power management

#### `src/ui.cpp` / `src/ui.h`
**User interface screens and interaction handling**
- Implements multiple screen states (keypad, settings, WiFi, etc.)
- Creates and manages LVGL UI elements and layouts
- Handles user input events and navigation
- Manages invoice overlay display and payment confirmation
- Provides message dialogs and status updates

**Screen Types:**
- `SCREEN_KEYPAD`: Main payment amount entry screen
- `SCREEN_SETTINGS`: Configuration and preferences
- `SCREEN_WIFI`: WiFi network selection and management
- `SCREEN_WIFI_PASSWORD`: Network password entry
- `SCREEN_INFO`: System information and diagnostics

**Key Functions:**
- `createKeypadScreen()`: Main POS interface
- `createInvoiceOverlay()`: Payment QR code display
- `updateInvoiceDisplay()`: Real-time invoice updates
- `showPaymentReceived()`: Payment confirmation UI

### Connectivity Modules

#### `src/wifi.cpp` / `src/wifi.h`
**WiFi connection and access point management**
- Manages WiFi station and AP mode operation
- Implements credential storage and auto-connection
- Provides network scanning and connection status monitoring
- Creates configuration web server in AP mode
- Integrates with Settings module for network persistence

**Key Functions:**
- `startConnection()`: Connect to saved or new network
- `startAPMode()`: Enable configuration access point
- `processScanResults()`: Handle network discovery
- `saveCredentials()`: Store network authentication
- `setStatusCallback()`: Integration with App coordinator

**AP Mode Configuration:**
- Creates web server for device setup
- Handles NWC URL configuration via web interface
- Provides WiFi network selection and credential entry

#### `src/nwc.cpp` / `src/nwc.h`
**Nostr Wallet Connect protocol implementation**
- Implements NWC protocol over WebSocket connections
- Handles Nostr event encryption/decryption (NIP-04)
- Manages Lightning invoice creation and payment monitoring
- Fetches Bitcoin price data from external APIs
- Provides automatic reconnection and connection health monitoring

**Key Functions:**
- `setupData()`: Parse NWC pairing URL and extract credentials
- `connectToRelay()`: Establish WebSocket connection to Nostr relay
- `makeInvoice()`: Create Lightning payment request
- `sendInvoiceLookupRequest()`: Check payment status
- `fetchBitcoinPrices()`: Update exchange rates
- `websocketEvent()`: Handle WebSocket events and Nostr messages

**Protocol Features:**
- Supports multiple fiat currencies (USD, EUR, GBP, CAD, CHF, AUD, JPY, CNY)
- Implements WebSocket fragmentation handling
- Provides connection health monitoring with automatic reconnection
- Handles encrypted Nostr events (kinds 23194, 23195, 23196, 23197)

### Configuration and Storage

#### `src/settings.cpp` / `src/settings.h`
**Application settings and persistent storage**
- Manages device configuration using ESP32 Preferences
- Handles shop name, currency selection, and AP password
- Provides PIN-based security for settings access
- Integrates with UI module for settings screens
- Supports factory reset functionality

**Key Functions:**
- `loadFromPreferences()`/`saveToPreferences()`: Persistence
- `setShopName()`/`getShopName()`: Merchant identification
- `setCurrency()`/`getCurrency()`: Payment currency selection
- `verifyPin()`: Security access control
- `resetToDefaults()`: Factory reset

#### `src/config.h`
**System constants and configuration definitions**
- Display resolution and buffer size constants
- WiFi timeout and connection parameters
- Screen state enumeration
- Hardware-specific configurations

### External Libraries

#### `lib/TFT_eSPI/`
**Enhanced display driver library**
- Comprehensive TFT display driver with touch support
- Multiple display controller support (ST7796, ILI9341, etc.)
- Font rendering and graphics primitives
- Hardware-specific optimizations for ESP32

#### `lib/aes/`
**AES encryption implementation**
- Provides cryptographic functions for secure communication
- Used by NWC module for Nostr message encryption

#### `lib/nostr/`
**Nostr protocol implementation**
- NIP-19 bech32 encoding/decoding (`nip19.cpp`)
- NIP-44 encryption for secure messaging (`nip44/`)
- Nostr event creation and verification (`nostr.cpp`)
- Integration with Bitcoin cryptographic functions

---

## Data Flow and Module Interactions

### 1. **Application Startup**
```
main.cpp → App::init() → Display::init() → Settings::init() → WiFiManager::init() → UI::init() → NWC::init()
```

### 2. **Payment Processing Flow**
```
UI (amount entry) → NWC::makeInvoice() → Nostr relay → Lightning wallet → 
Payment notification → NWC::handleNwcResponseEvent() → UI::showPaymentReceived()
```

### 3. **WiFi Configuration Flow**
```
UI (WiFi screen) → WiFiManager::startScan() → UI (network list) → 
WiFiManager::startConnection() → App::notifyWiFiStatusChanged() → NWC::connectToRelay()
```

### 4. **Settings Management Flow**
```
UI (settings screen) → Settings::setShopName() → Settings::saveToPreferences() → 
App state update → UI refresh
```

---

## Communication Protocols

### Nostr Wallet Connect (NWC)
- **WebSocket connection** to Nostr relay servers
- **Encrypted messaging** using NIP-04 encryption
- **Event types**: 23194 (requests), 23195 (responses), 23196/23197 (notifications)
- **Lightning integration** for invoice creation and payment monitoring

### Lightning Network
- **BOLT-11 invoices** for payment requests
- **Real-time payment notifications** via NWC protocol
- **Multi-currency support** with automatic satoshi conversion

### External APIs
- **Mempool.space API** for Bitcoin price data
- **NTP synchronization** for accurate timestamps
- **HTTPS requests** for secure API communication

---

## Hardware Configuration

### ESP32 Pin Mapping
- **GPIO 12**: Wake-up button for deep sleep
- **Display interface**: Parallel 8-bit bus (ST7796)
- **Touch interface**: I2C (FT5x06)
- **WiFi**: Built-in ESP32 radio

### Power Management
- **Deep sleep mode** with GPIO wake-up
- **Display backlight control** for power saving
- **30-second inactivity timeout** (configurable)
- **RTC GPIO configuration** for wake-up functionality

---

## Security Features

### Cryptographic Security
- **secp256k1 key pairs** for Nostr identity
- **AES encryption** for sensitive communications
- **PIN protection** for settings access
- **Secure storage** of credentials in ESP32 Preferences

### Network Security
- **HTTPS/WSS connections** for all external communications
- **Encrypted Nostr messaging** using NIP-04 standard
- **Access point password protection** for device configuration

---

## Development and Build

### PlatformIO Configuration
- **ESP32 development board** target
- **Arduino framework** with ESP-IDF components
- **Library dependencies** managed via platformio.ini
- **Build flags** for debugging and optimization

### Key Dependencies
- `lovyan03/LovyanGFX@^0.4.14`: Display driver
- `lvgl/lvgl@^8.1.0`: GUI framework
- `lnbits/Nostr@^0.2.0`: Nostr protocol implementation
- `micro-bitcoin/uBitcoin`: Bitcoin cryptographic functions
- `bblanchon/ArduinoJson@^6.21.0`: JSON processing
- `links2004/WebSockets@^2.3.7`: WebSocket client