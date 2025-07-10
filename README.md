# Nostr Wallet Connect Point of Sale Device

A Bitcoin Lightning point of Sale Device that uses NWC for wallet communications

## Overview

This is a Bitcoin Lightning Network point of sale device built for the Guition JC3248W535 3.5" ESP32-S3 development board with touch screen display. The device enables Bitcoin payments using Nostr Wallet Connect (NWC) protocol for wallet communications.

## Features

- **Lightning Network Payments**: Accept Bitcoin payments via Lightning Network
- **Touch Screen Interface**: 320x480 color touchscreen
- **Nostr Wallet Connect**: Wallet communication using NWC protocol
- **Multi-Currency Support**: USD, EUR, GBP, CAD, CHF, AUD, JPY, CNY with automatic Bitcoin price conversion
- **WiFi Setup**: Easy configuration on device with AP mode for NWC pairing code setup
- **PIN Protection**: Secure access to settings and configuration
- **Power Management**: Light sleep after a short period of inactivity and deep sleep mode after a longer period of inactivity  
- **QR Code Display**: Shows Lightning invoices for customer payments

## Hardware Requirements

- Guition JC3248W535 3.5" ESP32-S3 development board with touch screen display (320x480)
- Power supply (USB or battery)

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) installed
- Guition JC3248W535 3.5" touch screen display (320x480)

### Building and Flashing

```bash
# Clone the repository
git clone <repository-url>
cd nwc-pos

# Build the project
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```

### Initial Setup

1. Power on the device
2. Enter settings using the Settings icon at the bottom left corner of the keypad
3. Enter WiFi settings and connect to your WiFi network
4. In your NWC compatible wallet, create a new connection and copy the pairing code to your clip board
1. In settings, enable the NWC Pairing Code settings, use the default pin code "1234"
3. Connect to the WiFi AP created by the device and open the portal in your browser
4. Paste the pairing code you copied from your wallet
5. Set a new PIN number using the default pin code "1234"
6. Start accepting payments

## Architecture

The application uses a modular architecture with the following components:

- **App**: Central coordinator managing all modules
- **Display**: LVGL-based UI with LovyanGFX driver
- **WiFi**: Network connectivity management
- **NWC**: Nostr Wallet Connect protocol implementation
- **Settings**: Persistent configuration storage
- **UI**: User interface screens and interactions

## Development

Built with:
- PlatformIO Arduino framework
- LVGL graphics library
- LovyanGFX display driver
- Nostr protocol implementation
- WebSocket communication

## License

MIT License