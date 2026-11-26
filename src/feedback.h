#pragma once
#include <Arduino.h>

// RGB LED + hangszóró visszajelzés (nem blokkoló)

enum class LedMode : uint8_t {
  Off,
  Green,          // minden rendben (CAN forgalom van)
  YellowBlink,    // nincs CAN forgalom
  Red,            // CAN hiba / bus-off
  RedBlink,       // CAN driver nem indult
};

void fbBegin();
void fbLoop();                       // loop()-ból hívni
void fbSetMode(LedMode m);           // alap LED állapot
void fbKeyActive(bool active);       // gomb nyomva -> kék felülírja
void fbBeep(uint16_t freq, uint16_t ms);
void fbLedRaw(bool r, bool g, bool b);  // közvetlen (öntesztnél)
