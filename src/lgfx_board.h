#pragma once
// LovyanGFX konfiguráció a 2.8" ESP32-32E laphoz.
// Két panel-meghajtó van előkészítve (ILI9341 / ST7789), a választás futásidőben,
// az init() ELŐTT történik a selectPanel()-lel (beállításokból).

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "config.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _ili;
  lgfx::Panel_ST7789  _st;
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
    c.panel_width      = 240;
    c.panel_height     = 320;
    c.offset_x         = 0;
    c.offset_y         = 0;
    c.offset_rotation  = 0;
    c.dummy_read_pixel = 8;
    c.dummy_read_bits  = 1;
    c.readable         = true;
    c.invert           = invert;
    c.rgb_order        = bgr;
    c.dlen_16bit       = false;
    c.bus_shared       = false;
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
      c.bus_shared      = false;
      c.offset_rotation = 0;
      c.spi_host        = SPI3_HOST;  // VSPI (SD kártyát nem használunk)
      c.freq            = 1000000;
      c.pin_sclk        = PIN_TP_SCLK;
      c.pin_mosi        = PIN_TP_MOSI;
      c.pin_miso        = PIN_TP_MISO;
      c.pin_cs          = PIN_TP_CS;
      _touch.config(c);
    }
  }

  // init() előtt kell hívni!
  void selectPanel(bool st7789, bool invert, bool bgr) {
    if (st7789) {
      cfgPanel(_st, invert, bgr);
      setPanel(&_st);
    } else {
      cfgPanel(_ili, invert, bgr);
      setPanel(&_ili);
    }
  }
};
