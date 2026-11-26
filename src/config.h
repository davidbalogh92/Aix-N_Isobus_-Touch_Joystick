#pragma once
// =============================================================
//  AUX-N Touch Joystick - hardver konfiguráció
//  Lap: 2.8" ESP32-32E (E32R28T / E32R28T-1)
//  Forrás: lcdwiki.com/2.8inch_ESP32-32E_Display pin táblázat
// =============================================================

#define FW_NAME     "AUX-N Touch Joystick"
#define FW_VERSION  "0.1.0"
#define FW_CREDIT   "Készítette: Balogh Dávid & Claude"

// ---- LCD (HSPI) ----
#define PIN_LCD_SCLK   14
#define PIN_LCD_MOSI   13
#define PIN_LCD_MISO   12
#define PIN_LCD_DC      2
#define PIN_LCD_CS     15
#define PIN_LCD_BL     21   // háttérvilágítás, magas = be

// ---- Érintés XPT2046 (külön lábakon, VSPI) ----
#define PIN_TP_SCLK    25
#define PIN_TP_MOSI    32
#define PIN_TP_MISO    39
#define PIN_TP_CS      33
#define PIN_TP_IRQ     36

// ---- RGB LED (közös anód: LOW = világít) ----
#define PIN_LED_R      22
#define PIN_LED_G      16
#define PIN_LED_B      17

// ---- Hangszóró ----
#define PIN_SPK_EN      4   // LOW = erősítő engedélyezve
#define PIN_SPK        26

// ---- Gomb ----
#define PIN_BOOT        0   // BOOT gomb (LOW = nyomva)

// ---- CAN (SN65HVD230 modul) ----
//  TX: SPI periféria csatlakozó "CS" lába (IO27)
//  RX: Expand csatlakozó IO35 (csak bemenet)
#define PIN_CAN_TX     27
#define PIN_CAN_RX     35

// ---- Kijelző geometria (fekvő) ----
#define SCREEN_W      320
#define SCREEN_H      240
#define STATUS_H       24

// ---- Érintés szűrés ----
#define TOUCH_SAMPLE_MS        10   // mintavétel
#define TOUCH_PRESS_STABLE_MS  50   // ennyi ideig stabil érintés kell a lenyomáshoz
#define TOUCH_RELEASE_MS       30   // ennyi ideig nincs érintés -> elengedés
#define TOUCH_STABLE_RADIUS    18   // px, lenyomás előtti stabilitás
#define TOUCH_JUMP_RADIUS      30   // px, ennél nagyobb ugrás tartás közben = gyanús (2 ujj) -> elengedés
#define BUTTON_DEADZONE         6   // px, a gomb szélén (keret bordái alatt) nem reagál

// ---- Gombok ----
#define MAX_KEYS                8   // a későbbi AUX-N poolban is fixen 8 bemenet lesz
