#include "display.h"
#include "app.h"
#include "touch_debug.h"
#include <Arduino.h>

// Forward declarations for external UI elements from ui.cpp
namespace UI {
    extern lv_obj_t* getInvoiceLabel();
    extern lv_obj_t* getInvoiceSpinner();
}

namespace Display {
    // Global display instances
    Arduino_DataBus *bus = nullptr;
    Arduino_GFX *g = nullptr;
    Arduino_Canvas *gfx = nullptr;
    AXS15231B_Touch touch(Touch_SCL, Touch_SDA, Touch_INT, Touch_ADDR, TFT_rot);
    
    // Display configuration
    static const uint16_t screenWidth = TFT_WIDTH;
    static const uint16_t screenHeight = TFT_HEIGHT;
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t *buf = nullptr;
    
    // Touch coordinates
    static int32_t last_x = 0, last_y = 0;
    
    // Display objects
    static lv_obj_t* qr_canvas = nullptr;
    
    void init() {
        Serial.println("=== Initializing ArduinoGFX display ===");
        
        try {
            // Initialize ArduinoGFX display
            Serial.println("Creating bus...");
            bus = new Arduino_ESP32QSPI(TFT_CS, TFT_SCK, TFT_SDA0, TFT_SDA1, TFT_SDA2, TFT_SDA3);
            Serial.println("Creating display...");
            g = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, TFT_res_W, TFT_res_H);
            Serial.println("Creating canvas...");
            gfx = new Arduino_Canvas(TFT_res_W, TFT_res_H, g, 0, 0, TFT_rot);
            
            Serial.println("Starting display...");
            if (!gfx->begin(40000000UL)) {
                Serial.println("ERROR: Failed to initialize display!");
                return;
            }
            Serial.println("Display initialized successfully");
            
            // Turn on backlight first
            Serial.println("Turning on backlight...");
            pinMode(TFT_BL, OUTPUT);
            digitalWrite(TFT_BL, HIGH);
            
            // Clear display
            Serial.println("Clearing display...");
            gfx->fillScreen(BLACK);
            gfx->flush();
            Serial.println("Display cleared");
            
            // Run comprehensive touch diagnostics
            Serial.println("=== TOUCH DIAGNOSTICS ===");
            TouchDebug::scanI2C();
            TouchDebug::testInterruptPin();
            TouchDebug::directTouchTest();
            
            // Initialize touch controller
            Serial.println("Initializing touch controller...");
            Serial.printf("Touch pins - SDA:%d, SCL:%d, INT:%d, ADDR:0x%02X\n", Touch_SDA, Touch_SCL, Touch_INT, Touch_ADDR);
            
            if (!touch.begin()) {
                Serial.println("ERROR: Failed to initialize touch controller!");
                // Continue anyway for debugging
            } else {
                Serial.println("Touch controller initialized successfully");
                
                // Configure touch calibration
                touch.enOffsetCorrection(true);
                touch.setOffsets(Touch_X_min, Touch_X_max, TFT_res_W-1, Touch_Y_min, Touch_Y_max, TFT_res_H-1);
                Serial.println("Touch calibration configured");
            }
            
            // Initialize LVGL
            Serial.println("Initializing LVGL...");
            lv_init();
            Serial.println("LVGL initialized, setting up display driver...");
            setupLVGL();
            
            Serial.println("=== ArduinoGFX display initialization complete ===");
        } catch (...) {
            Serial.println("EXCEPTION during display initialization!");
        }
    }
    
    void cleanup() {
        // Clean up LVGL objects
        if (qr_canvas && lv_obj_is_valid(qr_canvas)) {
            lv_obj_del(qr_canvas);
            qr_canvas = nullptr;
        }
        
        // Clean up display buffer
        if (buf != nullptr) {
            heap_caps_free(buf);
            buf = nullptr;
        }
        
        // Touch cleanup handled automatically (stack object)
        if (gfx != nullptr) {
            delete gfx;
            gfx = nullptr;
        }
        if (g != nullptr) {
            delete g;
            g = nullptr;
        }
        if (bus != nullptr) {
            delete bus;
            bus = nullptr;
        }
        
        Serial.println("Display module cleaned up");
    }
    
    void setRotation(int rotation) {
        // ArduinoGFX rotation is set during Canvas creation
        // For runtime rotation changes, would need to recreate canvas
        Serial.printf("Rotation set to: %d\n", rotation);
    }
    
    void setupLVGL() {
        // Allocate display buffer using heap capabilities for optimal performance
        uint32_t bufSize = screenWidth * screenHeight / 10;
        buf = (lv_color_t*)heap_caps_malloc(bufSize * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        
        if (!buf) {
            Serial.println("ERROR: Failed to allocate LVGL display buffer!");
            return;
        }
        
        // Setup buffer to use for display
        lv_disp_draw_buf_init(&draw_buf, buf, nullptr, bufSize);

        // Setup & Initialize the display device driver
        static lv_disp_drv_t disp_drv;
        lv_disp_drv_init(&disp_drv);
        disp_drv.hor_res = screenWidth;
        disp_drv.ver_res = screenHeight;
        disp_drv.flush_cb = displayFlush;
        disp_drv.draw_buf = &draw_buf;
        lv_disp_drv_register(&disp_drv);

        // Setup & Initialize the input device driver
        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = touchpadRead;
        lv_indev_drv_register(&indev_drv);
        
        Serial.println("LVGL initialized with ArduinoGFX backend");
    }
    
    // Display callback to flush the buffer to screen
    void displayFlush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
        uint32_t w = lv_area_get_width(area);
        uint32_t h = lv_area_get_height(area);

        gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)color_p, w, h);
        gfx->flush();

        lv_disp_flush_ready(disp);
    }

    // Touchpad callback to read the touchpad
    void touchpadRead(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
        static int debug_counter = 0;
        
        // Debug: Print every 1000 calls to show function is being called
        if (debug_counter++ % 1000 == 0) {
            Serial.printf("Touch callback called %d times\n", debug_counter);
        }
        
        uint16_t touchX, touchY;
        if (touch.touched()) {
            // Read touched point from touch module
            touch.readData(&touchX, &touchY);

            // Set the coordinates
            data->point.x = touchX;
            data->point.y = touchY;
            data->state = LV_INDEV_STATE_PRESSED;
            last_x = touchX;
            last_y = touchY;
            
            Serial.println("TOUCH DETECTED - Pressed at " + String(touchX) + ", " + String(touchY));
            
            // Handle touch wake from light sleep
            App::handleTouchWake();
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    }
    
    // QR Code display function using LVGL QR code library
    void displayQRCode(const String& invoice) {
        Serial.println("=== Display::displayQRCode starting ===");
        Serial.println("Free heap: " + String(ESP.getFreeHeap()));
        Serial.println("Invoice length: " + String(invoice.length()));
        
        if (qr_canvas == nullptr || !lv_obj_is_valid(qr_canvas)) {
            Serial.println("ERROR: QR canvas not available");
            displayInvoiceTextFallback(invoice);
            return;
        }
        
        // Safety check - ensure invoice is not empty
        if (invoice.length() == 0) {
            Serial.println("ERROR: Empty invoice string");
            displayInvoiceTextFallback(invoice);
            return;
        }
        
        // Calculate QR code size - LVGL handles sizing automatically
        uint16_t qr_size = 280; // QR pixel size
        
        Serial.printf("Creating LVGL QR code with size %dx%d for %d character invoice\n", 
                     qr_size, qr_size, invoice.length());
        
        // Create LVGL QR code object with all parameters
        lv_obj_t* qr = lv_qrcode_create(qr_canvas, qr_size, lv_color_black(), lv_color_white());
        if (qr == nullptr) {
            Serial.println("ERROR: Failed to create LVGL QR code object");
            displayInvoiceTextFallback(invoice);
            return;
        }
        
        // Set QR code data - LVGL handles all the complexity
        lv_res_t result = lv_qrcode_update(qr, invoice.c_str(), invoice.length());
        if (result != LV_RES_OK) {
            Serial.printf("ERROR: Failed to update QR code data, result: %d\n", result);
            lv_obj_del(qr);
            displayInvoiceTextFallback(invoice);
            return;
        }
        
        // Center the QR code in the canvas
        lv_obj_center(qr);
        
        // Hide spinner and text label since we have QR code
        lv_obj_t* spinner = UI::getInvoiceSpinner();
        if (spinner != nullptr && lv_obj_is_valid(spinner)) {
            lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
        }
        
        lv_obj_t* label = UI::getInvoiceLabel();
        if (label != nullptr && lv_obj_is_valid(label)) {
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Show QR canvas
        lv_obj_clear_flag(qr_canvas, LV_OBJ_FLAG_HIDDEN);
        
        Serial.println("QR code successfully created and displayed using LVGL");
    }

    // Fallback function to display invoice as text
    void displayInvoiceTextFallback(const String& invoice) {
        lv_obj_t* label = UI::getInvoiceLabel();
        if (label == nullptr || !lv_obj_is_valid(label)) {
            return;
        }
        
        // Enhanced display for long invoices that can't be QR encoded
        String displayText;
        if (invoice.length() > 300) {
            displayText = "⚡ LIGHTNING INVOICE ⚡\n";
            displayText += String(invoice.length()) + " characters\n\n";
            displayText += "FULL INVOICE:\n";
            displayText += invoice;
            displayText += "\n\n📱 PAYMENT OPTIONS:\n";
            displayText += "• Copy invoice text\n";
            displayText += "• Use NFC/Bluetooth\n"; 
            displayText += "• Manual wallet paste\n\n";
            displayText += "⚠️ Too long for QR display";
        } else {
            displayText = "⚡ Lightning Invoice:\n\n";
            displayText += invoice;
            displayText += "\n\n📱 Scan with Lightning wallet";
        }
        
        lv_label_set_text(label, displayText.c_str());
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(label, 280);
        
        // Hide spinner
        lv_obj_t* spinner = UI::getInvoiceSpinner();
        if (spinner != nullptr && lv_obj_is_valid(spinner)) {
            lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Hide QR canvas since we're not using it
        if (qr_canvas != nullptr && lv_obj_is_valid(qr_canvas)) {
            lv_obj_add_flag(qr_canvas, LV_OBJ_FLAG_HIDDEN);
        }
        
        Serial.println("Text fallback display configured for long invoice");
    }
    
    // Getters and accessors
    Arduino_GFX& getGFX() {
        return *gfx;
    }
    
    lv_obj_t* getQRCanvas() {
        return qr_canvas;
    }
    
    void setQRCanvas(lv_obj_t* canvas) {
        qr_canvas = canvas;
    }
    
    void getLastTouch(int32_t& x, int32_t& y) {
        x = last_x;
        y = last_y;
    }
    
    // Power management functions
    void setBacklightBrightness(uint8_t brightness) {
        // ArduinoGFX doesn't have built-in brightness control
        // Use PWM on backlight pin for brightness control
        analogWrite(TFT_BL, brightness);
    }
    
    void turnOffBacklight() {
        Serial.println("Turning off display backlight");
        digitalWrite(TFT_BL, LOW);
    }
    
    void turnOnBacklight() {
        Serial.println("Turning on display backlight");
        digitalWrite(TFT_BL, HIGH);
    }
}