// MullerJoystickAuxO.h  (v2 - közös modul a permetező, szóró és vetőgép firmware-hez)
// Müller (ME) ISOBUS joystick AUX-O üzeneteinek fogadása.
// Készítette: Balogh Dávid & Claude
//
// A modul CSAK dekódol: gombszámot (0..23) és eseményt ad. Hogy melyik gomb mit csinál,
// azt minden gép a saját akciótáblájában dönti el (permetező: fix Müller kiosztás,
// szóró / vetőgép: menüben állítható). Ez a fájl mindhárom repóban AZONOS legyen
// (MULLER_JOY_VERSION alapján ellenőrizhető).
//
// Bemért és a gyári Müller Jobcomputerrel visszaellenőrzött protokoll (2026-11-25):
//   CAN ID : 0x1CE6FFxx  (prio 7, PGN 0xE600 VT->ECU, global, xx = joystick címe; mérve 0xCF)
//   Adat   : 21 nn FF FF cc 00 ss FF
//            21 = AUX-O (Auxiliary Input Type 1) állapot
//            nn = gombszám 0..23 = váltóállás*8 + belső index
//            cc = számláló-szerű bájt, nem használjuk
//            ss = 0 elengedve, 1 lenyomva (egyszer), 2 tartva (~200 ms-onként)
//   NAME   : gyártó 98 (Müller), function 128, device class 13 (identity eszközönként más)
//   Váltó  : 0 = felső (TRAIL), 1 = középső (permetezés), 2 = alsó (keret)
//   Belső index oszloponként: B1=0 B2=1 B3=2 B5=3 B4=4 B6=5 B7=6 B8=7
//
//   Gombok helye a joystickon:   B1 | B3 B4
//                                B2 | B5 B6
//                                   | B7 B8
//
// Biztonság: ha tartott gombnál HOLD_TIMEOUT_MS-ig nem jön "tartva" üzenet, vagy a joystick
// eltűnik a buszról, a modul magától Released eseményt küld -> hidraulika nem ragadhat be.
//
// Használat:
//   MullerJoystickAuxO joy;
//   joy.onEvent = [](uint8_t input, MullerJoyEvent e) { gepAkcio(input, e); };
//   CAN fogadásnál (minden 29 bites keret): joy.handleFrame(canId, data, len, millis());
//   loop()-ban:                              joy.update(millis());

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <functional>

#define MULLER_JOY_VERSION 2

enum class MullerJoyEvent : uint8_t { Pressed, Held, Released };

class MullerJoystickAuxO {
public:
  static constexpr uint32_t PGN_VT_TO_ECU = 0xE600;
  static constexpr uint32_t PGN_ADDRESS_CLAIM = 0xEE00;
  static constexpr uint8_t AUXO_STATUS = 0x21;
  static constexpr uint8_t NUM_INPUTS = 24;
  static constexpr uint8_t NO_INPUT = 0xFF;
  static constexpr uint16_t MANUFACTURER_MULLER = 98;
  static constexpr uint32_t HOLD_TIMEOUT_MS = 600;

  // ---- gombszám <-> váltóállás + fizikai gomb (B1..B8) ----
  static uint8_t layerOf(uint8_t input) { return input / 8; }           // 0 felső, 1 középső, 2 alsó
  static uint8_t buttonOf(uint8_t input) {                              // 1..8 (B1..B8)
    static const uint8_t idxToBtn[8] = {1, 2, 3, 5, 4, 6, 7, 8};
    return input < NUM_INPUTS ? idxToBtn[input % 8] : 0;
  }
  static uint8_t inputOf(uint8_t layer, uint8_t button) {               // layer 0..2, button 1..8
    static const uint8_t btnToIdx[9] = {0xFF, 0, 1, 2, 4, 3, 5, 6, 7};
    if (layer > 2 || button < 1 || button > 8) return NO_INPUT;
    return layer * 8 + btnToIdx[button];
  }
  static const char *layerName(uint8_t layer) {
    static const char *n[3] = {"Felso", "Kozepso", "Also"};
    return layer < 3 ? n[layer] : "?";
  }

  // A gyári Müller (permetező) jelentés - kijelzéshez, és a permetező fix kiosztásához.
  static const char *mullerDefaultName(uint8_t input) {
    static const char *n[NUM_INPUTS] = {
      "TRAIL kezi/auto", "TRAIL be/ki", "TRAIL szabad B3", "TRAIL szabad B5",
      "TRAIL szabad B4", "TRAIL szabad B6", "Kormanyzas balra", "Kormanyzas jobbra",
      "Celertek 100%", "Szakasz fokapcsolo", "Szakasz be jobbrol balra", "Szakasz ki balrol jobbra",
      "Szakasz be balrol jobbra", "Szakasz ki jobbrol balra", "Adag -", "Adag +",
      "Keret kezi/auto", "Keret szabad B2", "Keret emeles", "Keret sullyesztes",
      "Keret behajlitas", "Keret kihajlitas", "Bal fel, jobb le", "Jobb fel, bal le",
    };
    return input < NUM_INPUTS ? n[input] : "-";
  }

  // Müller joystick felismerése NAME alapján. A home made joystick Müller módban ugyanígy jelentkezik.
  static bool isMullerJoystickName(uint64_t name) {
    uint16_t manufacturer = (name >> 21) & 0x7FF;
    uint8_t function = (name >> 40) & 0xFF;
    uint8_t deviceClass = (name >> 49) & 0x7F;
    return manufacturer == MANUFACTURER_MULLER && function == 128 && deviceClass == 13;
  }

  std::function<void(uint8_t input, MullerJoyEvent e)> onEvent;

  bool enabled = true;          // a gép beállításából: ME joystick forrás be/ki
  uint8_t joystickAddress = 0xFF;  // 0xFF = még nem ismert (amíg nincs Address Claim, bárkit elfogad)

  // Minden fogadott 29 bites keretre meghívható. true, ha joystick állapotüzenet volt.
  bool handleFrame(uint32_t canId, const uint8_t *data, size_t len, uint32_t nowMs) {
    uint8_t sa = canId & 0xFF;
    uint8_t pf = (canId >> 16) & 0xFF;
    uint32_t pgn = (pf < 240) ? ((canId >> 8) & 0x3FF00) : ((canId >> 8) & 0x3FFFF);

    if (pgn == PGN_ADDRESS_CLAIM && len >= 8) {
      uint64_t name = 0;
      for (int i = 7; i >= 0; --i) name = (name << 8) | data[i];
      if (isMullerJoystickName(name)) joystickAddress = sa;
      else if (sa == joystickAddress) { releaseAll(); joystickAddress = 0xFF; }
      return false;
    }
    if (!enabled) return false;
    if (pgn != PGN_VT_TO_ECU || len < 7 || data[0] != AUXO_STATUS) return false;
    if (joystickAddress != 0xFF && sa != joystickAddress) return false;

    uint8_t input = data[1];
    uint8_t state = data[6];
    if (input >= NUM_INPUTS || state > 2) return false;

    Input &in = inputs_[input];
    if (state == 0) {
      if (in.down) { in.down = false; emit(input, MullerJoyEvent::Released); }
    } else {
      in.lastSeenMs = nowMs;
      if (!in.down) { in.down = true; emit(input, MullerJoyEvent::Pressed); }
      else if (state == 2) emit(input, MullerJoyEvent::Held);
    }
    return true;
  }

  void update(uint32_t nowMs) {
    for (uint8_t i = 0; i < NUM_INPUTS; ++i) {
      Input &in = inputs_[i];
      if (in.down && (uint32_t)(nowMs - in.lastSeenMs) > HOLD_TIMEOUT_MS) {
        in.down = false;
        emit(i, MullerJoyEvent::Released);
      }
    }
  }

  void releaseAll() {
    for (uint8_t i = 0; i < NUM_INPUTS; ++i)
      if (inputs_[i].down) { inputs_[i].down = false; emit(i, MullerJoyEvent::Released); }
  }

  bool isDown(uint8_t input) const { return input < NUM_INPUTS && inputs_[input].down; }

private:
  struct Input { bool down = false; uint32_t lastSeenMs = 0; };
  Input inputs_[NUM_INPUTS];
  void emit(uint8_t input, MullerJoyEvent e) { if (onEvent) onEvent(input, e); }
};
