#pragma once
// LovyanGFX konfiguráció az ESP32-32E kijelzős lapokhoz.
//  - E32R35T (3.5"): ST7796, érintés közös SPI buszon
//  - E32R28T (2.8"): ILI9341 vagy ST7789 (futásidőben választható), érintés külön buszon
// A panelválasztás az init() ELŐTT történik a selectPanel()-lel (beállításokból).

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "config.h"

class LGFX : public lgfx::LGFX_Device {
#if defined(BOARD_E32R35T)
  lgfx::Panel_ST7796  _p1;
#else
  lgfx::Panel_ILI9341 _p1;
  lgfx::Panel_ST7789  _p2;
#endif
  lgfx::Bus_SPI       _bus;
  lgfx::Light_PWM     _light;
  lgfx::Touch_XPT2046 _touch;

  template <class P>
  void cfgPanel(P& p, bool invert, bool bgr) {
    p.setBus(&_bus);
    auto c = p.config();
    c.pin_cs           = PIN_LCD_CS;
    c.pin_rst          = -1;      // RST az ESP32 EN-re van kötve
    c.pin_busy         = -1;
    c.panel_width      = PANEL_NATIVE_W;
    c.panel_height     = PANEL_NATIVE_H;
    c.offset_x         = 0;
    c.offset_y         = 0;
    c.offset_rotation  = 0;
    c.dummy_read_pixel = 8;
    c.dummy_read_bits  = 1;
    c.readable         = true;
    c.invert           = invert;
    c.rgb_order        = bgr;
    c.dlen_16bit       = false;
    c.bus_shared       = TOUCH_SHARED_BUS ? true : false;
    p.config(c);
    p.setLight(&_light);
    p.setTouch(&_touch);
  }

 public:
  LGFX() {
    {
      auto c = _bus.config();
      c.spi_host    = SPI2_HOST;   // HSPI
      c.spi_mode    = 0;
      c.freq_write  = 40000000;
      c.freq_read   = 16000000;
      c.spi_3wire   = false;
      c.use_lock    = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk    = PIN_LCD_SCLK;
      c.pin_mosi    = PIN_LCD_MOSI;
      c.pin_miso    = PIN_LCD_MISO;
      c.pin_dc      = PIN_LCD_DC;
      _bus.config(c);
    }
    {
      auto c = _light.config();
      c.pin_bl      = PIN_LCD_BL;
      c.invert      = false;
      c.freq        = 12000;
      c.pwm_channel = 7;
      _light.config(c);
    }
    {
      auto c = _touch.config();
      c.x_min           = 300;
      c.x_max           = 3900;
      c.y_min           = 200;
      c.y_max           = 3800;
      c.pin_int         = PIN_TP_IRQ;
      c.offset_rotation = 0;
#if TOUCH_SHARED_BUS
      c.bus_shared      = true;
      c.spi_host        = SPI2_HOST;  // ugyanaz a busz, mint az LCD
      c.freq            = 2500000;
#else
      c.bus_shared      = false;
      c.spi_host        = SPI3_HOST;  // VSPI (SD kártyát nem használunk)
      c.freq            = 1000000;
#endif
      c.pin_sclk        = PIN_TP_SCLK;
      c.pin_mosi        = PIN_TP_MOSI;
      c.pin_miso        = PIN_TP_MISO;
      c.pin_cs          = PIN_TP_CS;
      _touch.config(c);
    }
  }

  // init() előtt kell hívni! (3.5"-on az alt kapcsoló hatástalan: mindig ST7796)
  void selectPanel(bool alt, bool invert, bool bgr) {
#if defined(BOARD_E32R35T)
    (void)alt;
    cfgPanel(_p1, invert, bgr);
    setPanel(&_p1);
#else
    if (alt) { cfgPanel(_p2, invert, bgr); setPanel(&_p2); }
    else     { cfgPanel(_p1, invert, bgr); setPanel(&_p1); }
#endif
  }
};
