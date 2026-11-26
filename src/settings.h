#pragma once
#include <Arduino.h>

struct Settings {
  uint8_t  layout      = 6;      // 3, 6 vagy 8 gomb
  bool     panelSt7789 = false;  // false = ILI9341 (E32R28T), true = ST7789 (E32R28T-1)
  bool     invert      = false;  // színek invertálása
  bool     bgr         = false;  // színsorrend BGR
  uint8_t  brightness  = 192;    // 0..255
  bool     beep        = true;   // hangjelzés gombnyomásra
  uint8_t  rotation    = 1;      // 0 = álló, 1 = fekvő, 2 = álló 180°, 3 = fekvő 180°
  bool     calValid    = false;  // van-e érintés kalibráció
  uint16_t cal[8]      = {0};    // LovyanGFX kalibrációs adatok
};

extern Settings g_set;

void settingsLoad();
void settingsSave();
uint8_t nextLayout(uint8_t cur);   // 3 -> 6 -> 8 -> 3
uint8_t nextRotation(uint8_t cur); // fekvő -> álló -> fekvő 180° -> álló 180°
