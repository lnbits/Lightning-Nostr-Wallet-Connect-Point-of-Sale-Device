#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "pincfg.h"
#include "dispcfg.h"
#include "AXS15231B_touch.h"

// Portrait display constants
#define TFT_WIDTH   TFT_res_W
#define TFT_HEIGHT  TFT_res_H

namespace Display {
    // ArduinoGFX objects
    extern Arduino_DataBus *bus;
    extern Arduino_GFX *g;
    extern Arduino_Canvas *gfx;
    extern AXS15231B_Touch *touch;

    // Initialization and cleanup
    void init();
    void cleanup();
    void setRotation(int rotation);
    
    // Power management
    void setBacklightBrightness(uint8_t brightness); // 0-255
    void turnOffBacklight();
    void turnOnBacklight();
    
    // Display driver callbacks
    void displayFlush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
    void touchpadRead(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);
    
    // QR Code display functions
    void displayQRCode(const String& invoice);
    void displayInvoiceTextFallback(const String& invoice);
    
    // LVGL setup
    void setupLVGL();
    
    // Display object getters (for UI module to access)
    Arduino_GFX& getGFX();
    lv_obj_t* getQRCanvas();
    void setQRCanvas(lv_obj_t* canvas);
    
    // Touch coordinates
    void getLastTouch(int32_t& x, int32_t& y);
}