#pragma once
// Müller mód: gépprofilok és gombonkénti beállítás (hardverfüggetlen, natívan tesztelhető).
//
// A joystick mindig csak a gombszámot küldi; a funkciót a Jobcomputer dönti el. A beállítás ezért
// csak azt dönti el, melyik gomb LÁTSZIK / NYOMHATÓ, és milyen ikonnal. A küldött üzenet nem változik.
//
// Gombállapot (cfg[gombszám]):
//   BTN_OFF = rejtve (szürke, nem küld)
//   BTN_ON  = alap ikon
//   BTN_ALT = alternatív ikon (ha van; pl. TRAIL B4/B6: keret összecsukás/kinyitás <-> gyári tandem-híd ikon)
#include <stdint.h>
#include "muller_funcs.h"

enum : uint8_t { BTN_OFF = 0, BTN_ON = 1, BTN_ALT = 2 };
enum : uint8_t { PROF_TOWED_OWN = 0, PROF_TOWED_JB3 = 1, PROF_SELF = 2, PROF_CUSTOM = 3, PROF_COUNT_PRESET = 3 };

static constexpr uint8_t M_INPUTS = 24;

// Alternatív ikon / felirat (csak ahol van)
inline MIcon mullerAltIcon(uint8_t in) {
  if (in == 4) return MIcon::AxleIn;    // TRAIL B4
  if (in == 5) return MIcon::AxleOut;   // TRAIL B6
  return MIcon::None;
}
inline const char* mullerAltLabel(uint8_t in, bool en) {
  if (in == 4) return en ? "Axle \xE2\x86\x92\xE2\x86\x90" : "Híd \xE2\x86\x92\xE2\x86\x90";
  if (in == 5) return en ? "Axle \xE2\x86\x90\xE2\x86\x92" : "Híd \xE2\x86\x90\xE2\x86\x92";
  return "";
}

// Profil alkalmazása
inline void mullerApplyProfile(uint8_t prof, uint8_t cfg[M_INPUTS]) {
  for (uint8_t i = 0; i < M_INPUTS; i++) cfg[i] = mullerAssigned(i) ? BTN_ON : BTN_OFF;
  switch (prof) {
    case PROF_TOWED_JB3:          // gyári JB3: lakat be, B4/B6 tandem-híd ikonnal
      cfg[4] = BTN_ALT;
      cfg[5] = BTN_ALT;
      break;
    case PROF_SELF:               // önjáró: nincs vonórúd -> a teljes TRAIL fül rejtve
      for (uint8_t i = 0; i < 8; i++) cfg[i] = BTN_OFF;
      break;
    case PROF_TOWED_OWN:          // a felhasználó gépe: B3 (reteszelés) nincs
    default:
      cfg[2] = BTN_OFF;
      break;
  }
}

// Melyik profilnak felel meg a beállítás (ha egyiknek sem: egyéni)
inline uint8_t mullerProfileOf(const uint8_t cfg[M_INPUTS]) {
  uint8_t ref[M_INPUTS];
  for (uint8_t p = 0; p < PROF_COUNT_PRESET; p++) {
    mullerApplyProfile(p, ref);
    bool same = true;
    for (uint8_t i = 0; i < M_INPUTS && same; i++) same = (ref[i] == cfg[i]);
    if (same) return p;
  }
  return PROF_CUSTOM;
}

inline bool mullerBtnActive(const uint8_t cfg[M_INPUTS], uint8_t in) {
  return in < M_INPUTS && mullerAssigned(in) && cfg[in] != BTN_OFF;
}

inline MIcon mullerIconFor(const uint8_t cfg[M_INPUTS], uint8_t in) {
  if (in >= M_INPUTS) return MIcon::None;
  if (cfg[in] == BTN_ALT && mullerAltIcon(in) != MIcon::None) return mullerAltIcon(in);
  return mullerFunc(in).icon;
}

inline const char* mullerLabelFor(const uint8_t cfg[M_INPUTS], uint8_t in, bool en) {
  if (in >= M_INPUTS) return "";
  if (cfg[in] == BTN_ALT && mullerAltIcon(in) != MIcon::None) return mullerAltLabel(in, en);
  return en ? mullerFunc(in).en : mullerFunc(in).hu;
}

// Szerkesztőben a következő állapot: Be -> (Alt, ha van) -> Ki -> Be. Kiosztatlan gomb mindig Ki.
inline uint8_t mullerNextState(const uint8_t cfg[M_INPUTS], uint8_t in) {
  if (in >= M_INPUTS || !mullerAssigned(in)) return BTN_OFF;
  switch (cfg[in]) {
    case BTN_ON:  return (mullerAltIcon(in) != MIcon::None) ? BTN_ALT : BTN_OFF;
    case BTN_ALT: return BTN_OFF;
    default:      return BTN_ON;
  }
}

inline bool mullerLayerVisible(const uint8_t cfg[M_INPUTS], uint8_t layer) {
  for (uint8_t b = 1; b <= 8; b++)
    if (mullerBtnActive(cfg, mullerInputOf(layer, b))) return true;
  return false;
}

// Első látható fül (0xFF, ha egy sincs)
inline uint8_t mullerFirstVisible(const uint8_t cfg[M_INPUTS]) {
  for (uint8_t l = 0; l < 3; l++) if (mullerLayerVisible(cfg, l)) return l;
  return 0xFF;
}

// A cur utáni következő látható fül, körbe (cur maga is visszajöhet, ha csak az látható; 0xFF ha egy sincs)
inline uint8_t mullerNextVisible(const uint8_t cfg[M_INPUTS], uint8_t cur) {
  for (uint8_t k = 1; k <= 3; k++) {
    uint8_t l = (uint8_t)((cur + k) % 3);
    if (mullerLayerVisible(cfg, l)) return l;
  }
  return 0xFF;
}

inline const char* mullerProfileName(uint8_t p, bool en) {
  static const char* e[4] = {"Towed", "Towed JB3", "Self-prop.", "Custom"};
  static const char* h[4] = {"Vontatott", "Vont. JB3", "Önjáró", "Egyéni"};
  return p < 4 ? (en ? e[p] : h[p]) : "?";
}
