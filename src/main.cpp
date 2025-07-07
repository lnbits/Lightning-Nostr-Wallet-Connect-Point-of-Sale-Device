// Minimal display test for JC3248W535 hardware verification
#include <Arduino.h>
#include "pincfg.h"
#include "dispcfg.h"
#include <Arduino_GFX_Library.h>

// Display setup using ArduinoGFX
Arduino_DataBus *bus = new Arduino_ESP32QSPI(TFT_CS, TFT_SCK, TFT_SDA0, TFT_SDA1, TFT_SDA2, TFT_SDA3);
Arduino_GFX *g = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, TFT_res_W, TFT_res_H);
Arduino_Canvas *gfx = new Arduino_Canvas(TFT_res_W, TFT_res_H, g, 0, 0, TFT_rot);

void setup() {
    Serial.begin(115200);
    Serial.println("JC3248W535 Hardware Test Starting...");
    
    // Initialize display
    if (!gfx->begin(40000000UL)) {
        Serial.println("ERROR: Failed to initialize display!");
        return;
    }
    
    // Turn on backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    Serial.println("Backlight turned on");
    
    // Test basic display functionality
    Serial.println("Testing display colors...");
    
    // Fill with different colors to test display
    gfx->fillScreen(RED);
    gfx->flush();
    delay(1000);
    
    gfx->fillScreen(GREEN);
    gfx->flush();
    delay(1000);
    
    gfx->fillScreen(BLUE);
    gfx->flush();
    delay(1000);
    
    gfx->fillScreen(WHITE);
    gfx->flush();
    delay(1000);
    
    gfx->fillScreen(BLACK);
    gfx->flush();
    
    // Draw some basic shapes
    Serial.println("Drawing test patterns...");
    gfx->fillScreen(BLACK);
    gfx->setTextColor(WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(10, 10);
    gfx->println("Hardware Test OK!");
    gfx->setCursor(10, 50);
    gfx->println("Display: JC3248W535");
    gfx->setCursor(10, 90);
    gfx->println("Resolution: 320x480");
    
    // Draw some rectangles
    gfx->drawRect(10, 150, 100, 50, RED);
    gfx->fillRect(150, 150, 100, 50, GREEN);
    gfx->drawCircle(80, 300, 40, BLUE);
    gfx->fillCircle(200, 300, 40, YELLOW);
    
    gfx->flush();
    
    Serial.println("Hardware test complete!");
    Serial.println("If you see colors and text on display, hardware is working correctly.");
}

void loop() {
    // Simple animation to show display is responsive
    static int counter = 0;
    static unsigned long lastUpdate = 0;
    
    if (millis() - lastUpdate > 1000) {
        gfx->fillRect(10, 400, 200, 30, BLACK);
        gfx->setTextColor(WHITE);
        gfx->setTextSize(1);
        gfx->setCursor(10, 410);
        gfx->print("Counter: ");
        gfx->print(counter++);
        gfx->flush();
        lastUpdate = millis();
    }
    
    delay(10);
}