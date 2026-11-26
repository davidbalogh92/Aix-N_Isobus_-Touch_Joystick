#pragma once
// Müller (ME) joystick AUX-O KÜLDŐ - hardverfüggetlen (natívan tesztelhető).
// Referencia: docs/muller/TERV_muller_auxo.md (a CANable-es emulátorral, valódi Jobcomputeren kipróbálva).
//
//  Address Claim : ID 0x18EEFFsa, adat = NAME (little endian)
//  Állapot       : ID 0x1CE6FFsa, adat 21 nn FF FF cc 00 ss FF
//                  ss: 1 = lenyomva (egyszer), 2 = tartva (200 ms-onként), 0 = elengedve
//  Életjel       : ID 0x1CE7FFsa, adat FF 00 FF FF FF FF FF FF (1 mp-enként, ahogy az emulátor)
//  NAME          : gyártó 98, function 128, device class 13, industry group 2, self-configurable,
//                  saját identity (alap 0x1ABCDE -> A01A80000C5ABCDE)
//
// Egyszerre egy bemenet lehet aktív (rezisztív kijelző). Új lenyomás előtt az előzőt elengedi.
// Érvényes cím nélkül NEM küld állapotot; cím elvesztésekor az aktív bemenet megszűnik
// (a Jobcomputer a 200 ms-os ismétlés elmaradására magától leállítja).

#include <stdint.h>
#include <functional>

class MullerAuxOSender {
 public:
  using SendFn = std::function<bool(uint32_t id, const uint8_t* data, uint8_t len)>;

  static constexpr uint8_t  NO_INPUT        = 0xFF;
  static constexpr uint8_t  NUM_INPUTS      = 24;
  static constexpr uint32_t CLAIM_WAIT_MS   = 250;
  static constexpr uint32_t HOLD_REPEAT_MS  = 200;
  static constexpr uint32_t KEEPALIVE_MS    = 1000;
  static constexpr uint32_t DEFAULT_IDENTITY = 0x1ABCDE;
  static constexpr uint8_t  DEFAULT_ADDRESS = 0x90;

  enum class State : uint8_t { Off, Claiming, Claimed, CannotClaim };

  SendFn send;

  static uint64_t makeName(uint32_t identity);

  void begin(uint32_t identity, uint8_t preferredAddress, uint32_t nowMs);
  void stop(uint32_t nowMs);                       // elengedés + kikapcsolás

  // Minden fogadott 29 bites keret (address claim ütközés, request kezelés)
  void handleFrame(uint32_t canId, const uint8_t* data, uint8_t len, uint32_t nowMs);

  bool press(uint8_t input, uint32_t nowMs);       // false, ha nem küldhet (nincs cím / rossz szám)
  void release(uint32_t nowMs);                    // aktív bemenet elengedése (ss=0)
  void update(uint32_t nowMs);                     // loop()-ból

  State    state() const { return _state; }
  bool     claimed() const { return _state == State::Claimed; }
  uint8_t  address() const { return _sa; }
  uint8_t  activeInput() const { return _active; }
  uint64_t name() const { return _name; }

 private:
  void sendClaim();
  void sendStatus(uint8_t input, uint8_t ss);
  void sendKeepalive();
  bool pickNextAddress();

  State    _state = State::Off;
  uint64_t _name = 0;
  uint8_t  _sa = 0xFE;
  uint8_t  _preferred = DEFAULT_ADDRESS;
  uint32_t _claimT0 = 0;
  uint32_t _lastHoldTx = 0;
  uint32_t _lastKeepalive = 0;
  uint8_t  _active = NO_INPUT;
  uint8_t  _cc[NUM_INPUTS] = {0};
  uint32_t _usedMask[8] = {0};   // mások által foglalt címek (256 bit)
};
