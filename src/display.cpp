#include "display.h"
#include <Arduino.h>

// Forward declarations for external UI elements from ui.cpp
namespace UI {
    extern lv_obj_t* getInvoiceLabel();
    extern lv_obj_t* getInvoiceSpinner();
}

namespace Display {
    // Global display instance
    static LGFX lcd;
    
    // Display configuration
    static const uint16_t screenWidth = 320;
    static const uint16_t screenHeight = 480;
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf[screenWidth * 10];
    
    // Touch coordinates
    static int32_t last_x = 0, last_y = 0;
    
    // Display objects
    static lv_obj_t* qr_canvas = NULL;
    
    
    // LGFX Display Driver Class Implementation
    LGFX::LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.freq_write = 40000000;    
            cfg.pin_wr = 47;             
            cfg.pin_rd = -1;             
            cfg.pin_rs = 0;              

            // LCD data interface, 8bit MCU (8080)
            cfg.pin_d0 = 9;              
            cfg.pin_d1 = 46;             
            cfg.pin_d2 = 3;              
            cfg.pin_d3 = 8;              
            cfg.pin_d4 = 18;             
            cfg.pin_d5 = 17;             
            cfg.pin_d6 = 16;             
            cfg.pin_d7 = 15;             

            _bus_instance.config(cfg);   
            _panel_instance.setBus(&_bus_instance);      
        }

        { 
            auto cfg = _panel_instance.config();    

            cfg.pin_cs           =    -1;  
            cfg.pin_rst          =    4;  
            cfg.pin_busy         =    -1; 

            cfg.panel_width      =   TFT_WIDTH;
            cfg.panel_height     =   TFT_HEIGHT;
            cfg.offset_x         =     0;
            cfg.offset_y         =     0;
            cfg.offset_rotation  =     0;
            cfg.dummy_read_pixel =     8;
            cfg.dummy_read_bits  =     1;
            cfg.readable         =  false;
            cfg.invert           = true;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;

            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();    

            cfg.pin_bl = 45;              
            cfg.invert = false;           
            cfg.freq   = 44100;           
            cfg.pwm_channel = 7;          

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);  
        }

        { 
            auto cfg = _touch_instance.config();

            cfg.x_min      = 0;
            cfg.x_max      = 319;
            cfg.y_min      = 0;  
            cfg.y_max      = 479;
            cfg.pin_int    = 7;  
            cfg.bus_shared = true; 
            cfg.offset_rotation = 0;

            cfg.i2c_port = 1;
            cfg.i2c_addr = 0x38;
            cfg.pin_sda  = 6;   
            cfg.pin_scl  = 5;   
            cfg.freq = 400000;  

            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);  
        }

        setPanel(&_panel_instance); 
    }
    

    void init() {
        lcd.init();
        lv_init();
        setRotation(2);
        setupLVGL();
    }
    
    void cleanup() {
        // Clean up LVGL objects
        if (qr_canvas && lv_obj_is_valid(qr_canvas)) {
            lv_obj_del(qr_canvas);
            qr_canvas = NULL;
        }
        
        // Serial.println("Display module cleaned up");
    }
    
    void setRotation(int rotation) {
        lcd.setRotation(rotation);
    }
    
    void setupLVGL() {
        // Setup buffer to use for display
        lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

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
    }
    
    // Display callback to flush the buffer to screen
    void displayFlush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
        uint32_t w = (area->x2 - area->x1 + 1);
        uint32_t h = (area->y2 - area->y1 + 1);

        lcd.startWrite();
        lcd.setAddrWindow(area->x1, area->y1, w, h);
        lcd.pushPixels((uint16_t *)&color_p->full, w * h, true);
        lcd.endWrite();

        lv_disp_flush_ready(disp);
    }

    // Touchpad callback to read the touchpad
    void touchpadRead(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
        uint16_t touchX, touchY;
        bool touched = lcd.getTouch(&touchX, &touchY);

        if (!touched) {
            data->state = LV_INDEV_STATE_REL;
        } else {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touchX;
            data->point.y = touchY;
            last_x = touchX;
            last_y = touchY;
        }
    }
    
    // QR Code display function using LVGL QR code library
    void displayQRCode(const String& invoice) {
        Serial.println("=== Display::displayQRCode starting ===");
        Serial.println("Free heap: " + String(ESP.getFreeHeap()));
        Serial.println("Invoice length: " + String(invoice.length()));
        
        if (qr_canvas == NULL || !lv_obj_is_valid(qr_canvas)) {
            // Serial.println("ERROR: QR canvas not available");
            displayInvoiceTextFallback(invoice);
            return;
        }
        
        // Safety check - ensure invoice is not empty
        if (invoice.length() == 0) {
            // Serial.println("ERROR: Empty invoice string");
            displayInvoiceTextFallback(invoice);
            return;
        }
        
        // Calculate QR code size - LVGL handles sizing automatically
        uint16_t qr_size = 280; // QR pixel size
        
        Serial.printf("Creating LVGL QR code with size %dx%d for %d character invoice\n", 
                     qr_size, qr_size, invoice.length());
        
        // Create LVGL QR code object with all parameters
        lv_obj_t* qr = lv_qrcode_create(qr_canvas, qr_size, lv_color_black(), lv_color_white());
        if (qr == NULL) {
            // Serial.println("ERROR: Failed to create LVGL QR code object");
            displayInvoiceTextFallback(invoice);
            return;
        }
        
        // Set QR code data - LVGL handles all the complexity
        lv_res_t result = lv_qrcode_update(qr, invoice.c_str(), invoice.length());
        if (result != LV_RES_OK) {
            // Serial.printf("ERROR: Failed to update QR code data, result: %d\n", result);
            lv_obj_del(qr);
            displayInvoiceTextFallback(invoice);
            return;
        }
        
        // Center the QR code in the canvas
        lv_obj_center(qr);
        
        // Hide spinner and text label since we have QR code
        lv_obj_t* spinner = UI::getInvoiceSpinner();
        if (spinner != NULL && lv_obj_is_valid(spinner)) {
            lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
        }
        
        lv_obj_t* label = UI::getInvoiceLabel();
        if (label != NULL && lv_obj_is_valid(label)) {
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Show QR canvas
        lv_obj_clear_flag(qr_canvas, LV_OBJ_FLAG_HIDDEN);
        
        Serial.println("QR code successfully created and displayed using LVGL");
    }

    // Fallback function to display invoice as text
    void displayInvoiceTextFallback(const String& invoice) {
        lv_obj_t* label = UI::getInvoiceLabel();
        if (label == NULL || !lv_obj_is_valid(label)) {
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
        if (spinner != NULL && lv_obj_is_valid(spinner)) {
            lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Hide QR canvas since we're not using it
        if (qr_canvas != NULL && lv_obj_is_valid(qr_canvas)) {
            lv_obj_add_flag(qr_canvas, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Serial.println("Text fallback display configured for long invoice");
    }
    
    // Getters and accessors
    LGFX& getLCD() {
        return lcd;
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
        lcd.setBrightness(brightness);
    }
    
    void turnOffBacklight() {
        Serial.println("Turning off display backlight");
        lcd.setBrightness(0);
    }
    
    void turnOnBacklight() {
        Serial.println("Turning on display backlight");
        lcd.setBrightness(255); // Full brightness
    }
}