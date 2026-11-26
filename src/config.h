#pragma once
// =============================================================
//  AUX-N Touch Joystick - hardver konfiguráció
//  Lapok (platformio.ini env választja):
//    BOARD_E32R35T : 3.5" ESP32-32E (ST7796U, 320x480)  <- fő cél
//    BOARD_E32R28T : 2.8" ESP32-32E (ILI9341V / ST7789, 240x320)
//  Forrás: lcdwiki.com pin táblázatok
// =============================================================

#define FW_NAME     "AUX-N Touch Joystick"
#define FW_VERSION  "0.7.0"
#define FW_CREDIT   "Készítette: Balogh Dávid & Claude"

#if !defined(BOARD_E32R35T) && !defined(BOARD_E32R28T)
#define BOARD_E32R35T
#endif

// ---- közös: RGB LED (közös anód: LOW = világít), hang, BOOT ----
#define PIN_LED_R      22
#define PIN_LED_G      16
#define PIN_LED_B      17
#define PIN_SPK_EN      4   // LOW = erősítő engedélyezve
#define PIN_SPK        26
#define PIN_BOOT        0   // LOW = nyomva

// ---- LCD SPI (mindkét lapon) ----
#define PIN_LCD_SCLK   14
#define PIN_LCD_MOSI   13
#define PIN_LCD_MISO   12
#define PIN_LCD_DC      2
#define PIN_LCD_CS     15

#if defined(BOARD_E32R35T)
  #define BOARD_NAME     "E32R35T 3.5\""
  #define PIN_LCD_BL     27   // háttérfény, magas = be
  // Érintés: KÖZÖS SPI busz az LCD-vel
  #define TOUCH_SHARED_BUS 1
  #define PIN_TP_SCLK    PIN_LCD_SCLK
  #define PIN_TP_MOSI    PIN_LCD_MOSI
  #define PIN_TP_MISO    PIN_LCD_MISO
  #define PIN_TP_CS      33
  #define PIN_TP_IRQ     36
  // CAN: TX = I2C csatlakozó SCL (IO25), RX = Expand IO35
  #define PIN_CAN_TX     25
  #define PIN_CAN_RX     35
  #define PANEL_NATIVE_W 320
  #define PANEL_NATIVE_H 480
  #define STATUS_H       30
#else
  #define BOARD_NAME     "E32R28T 2.8\""
  #define PIN_LCD_BL     21
  // Érintés: külön lábakon (VSPI)
  #define TOUCH_SHARED_BUS 0
  #define PIN_TP_SCLK    25
  #define PIN_TP_MOSI    32
  #define PIN_TP_MISO    39
  #define PIN_TP_CS      33
  #define PIN_TP_IRQ     36
  // CAN: TX = SPI periféria csatlakozó CS (IO27), RX = Expand IO35
  #define PIN_CAN_TX     27
  #define PIN_CAN_RX     35
  #define PANEL_NATIVE_W 240
  #define PANEL_NATIVE_H 320
  #define STATUS_H       24
#endif

// ---- Fényérzékelő (R21 fotoellenállás) - a pontos láb a betanításkor derül ki ----
#define PIN_LIGHT_A    34   // a gyártói leírás szerint akku ADC; hasonló lapokon itt az LDR
#define PIN_LIGHT_B    39   // Expand bemenet
#define AUTO_BRIGHT_MIN 24  // Auto módban a legkisebb fényerő (ne sötétüljön el)
#define AUTO_BRIGHT_MAX 255

// ---- Érintés szűrés ----
#define TOUCH_SAMPLE_MS        10   // mintavétel
#define TOUCH_PRESS_STABLE_MS  50   // ennyi ideig stabil érintés kell a lenyomáshoz
#define TOUCH_RELEASE_MS       30   // ennyi ideig nincs érintés -> elengedés
#define TOUCH_STABLE_RADIUS    18   // px, lenyomás előtti stabilitás
#define TOUCH_JUMP_RADIUS      30   // px, ennél nagyobb ugrás tartás közben = gyanús (2 ujj) -> elengedés
#define BUTTON_DEADZONE         6   // px, a gomb szélén (keret bordái alatt) nem reagál

// ---- Gombok ----
#define MAX_KEYS                8   // a későbbi AUX-N poolban is fixen 8 bemenet lesz
