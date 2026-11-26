#pragma once
// Müller mód: szabadon összerakható kiosztás (3 fül x 8 hely). Hardverfüggetlen, natívan tesztelhető.
//
// Minden helyre (fül, B1..B8) bármelyik Müller funkció (gombszám 0..23) kerülhet, vagy üres.
// A Jobcomputer csak a gombszámot nézi (az a váltóállást is tartalmazza), ezért a hely mindegy.
//
// Hely kód (uint8):  0xFF = üres;  bit0..4 = gombszám (0..23);  bit7 = alternatív ikon (TRAIL 4/5: tandem-híd)
#include <stdint.h>
#include "muller_funcs.h"

static constexpr uint8_t SLOT_EMPTY = 0xFF;
static constexpr uint8_t SLOT_ALT   = 0x80;
static constexpr uint8_t TAB_NAMES  = 8;

inline uint8_t slotInput(uint8_t code) { return code == SLOT_EMPTY ? 0xFF : (uint8_t)(code & 0x1F); }
inline bool    slotAlt(uint8_t code)   { return code != SLOT_EMPTY && (code & SLOT_ALT); }

inline bool slotActive(uint8_t code) {
  uint8_t in = slotInput(code);
  return in < 24 && mullerAssigned(in);
}

inline MIcon slotAltIcon(uint8_t in) {
  if (in == 4) return MIcon::AxleIn;
  if (in == 5) return MIcon::AxleOut;
  return MIcon::None;
}

inline MIcon slotIcon(uint8_t code) {
  if (!slotActive(code)) return MIcon::None;
  uint8_t in = slotInput(code);
  if (slotAlt(code) && slotAltIcon(in) != MIcon::None) return slotAltIcon(in);
  return mullerFunc(in).icon;
}

inline const char* slotLabel(uint8_t code, bool en) {
  if (!slotActive(code)) return "";
  uint8_t in = slotInput(code);
  if (slotAlt(code) && slotAltIcon(in) != MIcon::None) {
    if (in == 4) return en ? "Axle \xE2\x86\x92\xE2\x86\x90" : "Híd \xE2\x86\x92\xE2\x86\x90";
    return en ? "Axle \xE2\x86\x90\xE2\x86\x92" : "Híd \xE2\x86\x90\xE2\x86\x92";
  }
  return en ? mullerFunc(in).en : mullerFunc(in).hu;
}

// Gyári Müller kiosztás a felhasználó gépére (TRAIL B3 = lakat nincs -> üres), gyári fülnevek
inline void layoutDefault(uint8_t slots[3][8], uint8_t names[3]) {
  for (uint8_t l = 0; l < 3; l++) {
    for (uint8_t b = 0; b < 8; b++) {
      uint8_t in = mullerInputOf(l, b + 1);
      slots[l][b] = mullerAssigned(in) ? in : SLOT_EMPTY;
    }
    names[l] = l;   // TRAIL, SPRAY, BOOM
  }
  slots[0][2] = SLOT_EMPTY;   // TRAIL B3
}

inline bool layoutTabVisible(const uint8_t slots[3][8], uint8_t tab) {
  for (uint8_t b = 0; b < 8; b++) if (slotActive(slots[tab][b])) return true;
  return false;
}

inline uint8_t layoutFirstVisible(const uint8_t slots[3][8]) {
  for (uint8_t t = 0; t < 3; t++) if (layoutTabVisible(slots, t)) return t;
  return 0xFF;
}

inline uint8_t layoutNextVisible(const uint8_t slots[3][8], uint8_t cur) {
  for (uint8_t k = 1; k <= 3; k++) {
    uint8_t t = (uint8_t)((cur + k) % 3);
    if (layoutTabVisible(slots, t)) return t;
  }
  return 0xFF;
}

// Fülnevek (index a listában)
inline const char* tabName(uint8_t idx, bool en) {
  static const char* e[TAB_NAMES] = {"TRAIL", "SPRAY", "BOOM", "1", "2", "3", "MAIN", "EXTRA"};
  static const char* h[TAB_NAMES] = {"TRAIL", "Permetezés", "Keret", "1", "2", "3", "FŐ", "EXTRA"};
  return idx < TAB_NAMES ? (en ? e[idx] : h[idx]) : "?";
}

// Funkcióválasztó: a csoport (gyári oldal) választható elemei. Visszaadja a darabszámot (max 12).
// Sorrend: B1..B8 a gyári oldalon (kiosztatlanok kihagyva), a TRAIL csoport végén a 2 alternatív ikon.
inline uint8_t pickerItems(uint8_t group, uint8_t out[12]) {
  uint8_t n = 0;
  for (uint8_t b = 1; b <= 8; b++) {
    uint8_t in = mullerInputOf(group, b);
    if (in < 24 && mullerAssigned(in)) out[n++] = in;
  }
  if (group == 0) {
    out[n++] = (uint8_t)(4 | SLOT_ALT);
    out[n++] = (uint8_t)(5 | SLOT_ALT);
  }
  return n;
}

// Régi (v0.5–0.6) gombonkénti állapotból (0 ki, 1 be, 2 alt) átalakítás
inline void layoutFromLegacy(const uint8_t cfg[24], uint8_t slots[3][8]) {
  for (uint8_t l = 0; l < 3; l++)
    for (uint8_t b = 0; b < 8; b++) {
      uint8_t in = mullerInputOf(l, b + 1);
      uint8_t st = cfg[in];
      if (!mullerAssigned(in) || st == 0) slots[l][b] = SLOT_EMPTY;
      else slots[l][b] = (st == 2 && slotAltIcon(in) != MIcon::None) ? (uint8_t)(in | SLOT_ALT) : in;
    }
}
