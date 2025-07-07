// Touch debugging utilities
#include "pincfg.h"
#include "dispcfg.h"
#include <Wire.h>
#include <Arduino.h>

namespace TouchDebug {
    
void scanI2C() {
    Serial.println("=== I2C Scanner ===");
    Serial.printf("Scanning I2C on SDA:%d, SCL:%d\n", Touch_SDA, Touch_SCL);
    
    Wire.begin(Touch_SDA, Touch_SCL);
    
    int deviceCount = 0;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        byte error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.printf("I2C device found at address 0x%02X\n", addr);
            deviceCount++;
        }
    }
    
    if (deviceCount == 0) {
        Serial.println("No I2C devices found!");
    } else {
        Serial.printf("Found %d I2C device(s)\n", deviceCount);
    }
    
    // Specifically test touch controller address
    Serial.printf("Testing touch controller at 0x%02X...\n", Touch_ADDR);
    Wire.beginTransmission(Touch_ADDR);
    byte result = Wire.endTransmission();
    Serial.printf("Touch controller response: %s (error code: %d)\n", 
                  result == 0 ? "FOUND" : "NOT FOUND", result);
}

void testInterruptPin() {
    Serial.println("=== Interrupt Pin Test ===");
    Serial.printf("Testing interrupt pin %d\n", Touch_INT);
    
    pinMode(Touch_INT, INPUT_PULLUP);
    int pinState = digitalRead(Touch_INT);
    Serial.printf("Interrupt pin state: %s (%d)\n", pinState ? "HIGH" : "LOW", pinState);
    
    // Test for 5 seconds
    Serial.println("Touch the screen now - monitoring interrupt pin for 5 seconds...");
    unsigned long startTime = millis();
    int lastState = pinState;
    int changeCount = 0;
    
    while (millis() - startTime < 5000) {
        int currentState = digitalRead(Touch_INT);
        if (currentState != lastState) {
            changeCount++;
            Serial.printf("Pin change detected: %s\n", currentState ? "HIGH" : "LOW");
            lastState = currentState;
        }
        delay(10);
    }
    
    Serial.printf("Interrupt pin changes detected: %d\n", changeCount);
}

void directTouchTest() {
    Serial.println("=== Direct Touch Controller Test ===");
    
    Wire.begin(Touch_SDA, Touch_SCL);
    
    // Try to read from touch controller using the command from working code
    uint8_t readCmd[8] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08};
    uint8_t response[8] = {0};
    
    Wire.beginTransmission(Touch_ADDR);
    Wire.write(readCmd, sizeof(readCmd));
    byte writeResult = Wire.endTransmission();
    
    Serial.printf("Write command result: %d\n", writeResult);
    
    int bytesReceived = Wire.requestFrom(Touch_ADDR, (uint8_t)sizeof(response));
    Serial.printf("Bytes received: %d\n", bytesReceived);
    
    for (int i = 0; i < bytesReceived && i < sizeof(response); i++) {
        response[i] = Wire.read();
    }
    
    Serial.print("Response data: ");
    for (int i = 0; i < sizeof(response); i++) {
        Serial.printf("0x%02X ", response[i]);
    }
    Serial.println();
}

} // namespace TouchDebug