#pragma once
#include <Arduino.h>

// 1. fázis: a CAN csak "hallgat" (normál módban, tehát ACK-ol, de NEM küld semmit).
// Megmutatja, hogy él-e a busz, hány eszköz forgalmaz, és látszik-e a VT (terminál).

enum class CanState : uint8_t {
  DriverFail,   // a TWAI driver nem indult
  NoTraffic,    // fut, de nincs forgalom
  Ok,           // van forgalom
  ErrorPassive, // sok hiba
  BusOff,       // busz leválasztva (helyreállítás folyamatban)
};

struct CanStats {
  CanState state      = CanState::DriverFail;
  uint32_t fps        = 0;     // frame / másodperc
  uint8_t  nodes      = 0;     // különböző forráscímek száma (utolsó ~2 mp)
  bool     vtSeen     = false; // VT Status (PGN 0xFE6E) az utolsó 3 mp-ben
  uint8_t  vtAddr     = 0xFF;
  uint32_t txErr      = 0;
  uint32_t rxErr      = 0;
  uint32_t totalRx    = 0;
};

bool canBegin();
void canLoop();
const CanStats& canStats();
const char* canStateText(CanState s);
