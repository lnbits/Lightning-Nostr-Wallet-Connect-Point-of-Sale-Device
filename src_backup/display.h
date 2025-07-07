#pragma once

#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lvgl.h>

// Portrait display constants
#define TFT_WIDTH   320
#define TFT_HEIGHT  480

namespace Display {
    // LGFX Display Driver Class
    class LGFX : public lgfx::LGFX_Device {
        lgfx::Panel_ST7796  _panel_instance;
        lgfx::Bus_Parallel8 _bus_instance;
        lgfx::Light_PWM     _light_instance;
        lgfx::Touch_FT5x06  _touch_instance;

    public:
        LGFX(void);
    };

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
    void displayQRCodeAlternative(const String& invoice);
    void displayInvoiceTextFallback(const String& invoice);
    void testCanvasDrawing();
    
    
    // LVGL setup
    void setupLVGL();
    
    // Display object getters (for UI module to access)
    LGFX& getLCD();
    lv_obj_t* getQRCanvas();
    void setQRCanvas(lv_obj_t* canvas);
    
    // Touch coordinates
    void getLastTouch(int32_t& x, int32_t& y);
}