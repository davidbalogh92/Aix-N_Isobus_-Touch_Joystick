#pragma once
#include <Arduino.h>

// A CAN normál módban fut (ACK-ol). Magától nem küld; csak a Müller mód küld a canSend()-del.
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

// Minden fogadott 29 bites keret továbbadása (pl. Müller küldő: address claim / request kezelés)
typedef void (*CanRxHook)(uint32_t id, const uint8_t* data, uint8_t len);
void canSetRxHook(CanRxHook h);

// 29 bites keret küldése (nem blokkol; false, ha nem sikerült sorba tenni)
bool canSend(uint32_t id, const uint8_t* data, uint8_t len);

bool canBegin();
void canLoop();
const CanStats& canStats();
const char* canStateText(CanState s);
