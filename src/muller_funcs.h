#pragma once
// Müller (ME) permetező joystick funkciótábla - a kijelzőhöz (ikon + felirat HU/EN).
// Gombszám = váltóállás * 8 + belső index; belső index: B1=0 B2=1 B3=2 B5=3 B4=4 B6=5 B7=6 B8=7
// Forrás: docs/muller/ (bemért) + a felhasználó gépének beállítása:
//   TRAIL B5 = vonórúd középre (tartásos), TRAIL B4 = keret összecsukás, TRAIL B6 = keret kinyitás
//   (hivatalosan tandem, kormányzott hátsó hidas gépen ezek a híd aktiválása/reteszelése).
#include <stdint.h>

enum class MIcon : uint8_t {
  None,
  Target100, SectMain, SectOnRL, SectOnLR, SectOffLR, SectOffRL, RateMinus, RatePlus,
  BoomAuto, BoomUp, BoomDown, FoldIn, FoldOut, TiltLeftUp, TiltRightUp,
  TrailAuto, TrailOnOff, TrailCenter, SteerLeft, SteerRight, AxleLock,
  AxleIn, AxleOut,   // gyári JB3 tandem-híd ikonok (alternatív ikon a TRAIL B4/B6-on)
};

struct MFunc {
  MIcon       icon;
  const char* hu;      // rövid magyar felirat
  const char* en;      // rövid angol felirat
  bool        hold;    // tartásos (mozgató) funkció - kijelzéshez
};

inline const MFunc& mullerFunc(uint8_t input) {
  static const MFunc T[25] = {
    /* 0  TRAIL B1 */ {MIcon::TrailAuto,   "Auto/kézi",   "Auto/manual", false},
    /* 1  TRAIL B2 */ {MIcon::TrailOnOff,  "TRAIL be/ki", "TRAIL on/off", false},
    /* 2  TRAIL B3 */ {MIcon::AxleLock,    "Reteszelés",  "Axle lock",   false},
    /* 3  TRAIL B5 */ {MIcon::TrailCenter, "Középre",     "Center",      true},
    /* 4  TRAIL B4 */ {MIcon::FoldIn,      "Összecsuk",   "Fold",        true},
    /* 5  TRAIL B6 */ {MIcon::FoldOut,     "Kinyit",      "Unfold",      true},
    /* 6  TRAIL B7 */ {MIcon::SteerLeft,   "Balra",       "Steer left",  true},
    /* 7  TRAIL B8 */ {MIcon::SteerRight,  "Jobbra",      "Steer right", true},
    /* 8  SPRAY B1 */ {MIcon::Target100,   "Célérték",    "Target",      false},
    /* 9  SPRAY B2 */ {MIcon::SectMain,    "Főkapcsoló",  "Main switch", false},
    /* 10 SPRAY B3 */ {MIcon::SectOnRL,    "Be \xE2\x86\x90",  "On \xE2\x86\x90",  false},
    /* 11 SPRAY B5 */ {MIcon::SectOffLR,   "Ki \xE2\x86\x92",  "Off \xE2\x86\x92", false},
    /* 12 SPRAY B4 */ {MIcon::SectOnLR,    "Be \xE2\x86\x92",  "On \xE2\x86\x92",  false},
    /* 13 SPRAY B6 */ {MIcon::SectOffRL,   "Ki \xE2\x86\x90",  "Off \xE2\x86\x90", false},
    /* 14 SPRAY B7 */ {MIcon::RateMinus,   "Adag -",      "Rate -",      false},
    /* 15 SPRAY B8 */ {MIcon::RatePlus,    "Adag +",      "Rate +",      false},
    /* 16 BOOM  B1 */ {MIcon::BoomAuto,    "Auto/kézi",   "Auto/manual", false},
    /* 17 BOOM  B2 */ {MIcon::None,        "",            "",            false},
    /* 18 BOOM  B3 */ {MIcon::BoomUp,      "Emelés",      "Raise",       true},
    /* 19 BOOM  B5 */ {MIcon::BoomDown,    "Süllyesztés", "Lower",       true},
    /* 20 BOOM  B4 */ {MIcon::FoldIn,      "Behajlítás",  "Angle in",    true},
    /* 21 BOOM  B6 */ {MIcon::FoldOut,     "Kihajlítás",  "Angle out",   true},
    /* 22 BOOM  B7 */ {MIcon::TiltLeftUp,  "Bal fel",     "Left up",     true},
    /* 23 BOOM  B8 */ {MIcon::TiltRightUp, "Jobb fel",    "Right up",    true},
    /* --        */ {MIcon::None,        "",            "",            false},
  };
  return T[input < 24 ? input : 24];
}

inline bool mullerAssigned(uint8_t input) { return mullerFunc(input).icon != MIcon::None; }

// layer 0..2 (0 = felső/TRAIL, 1 = középső/permetezés, 2 = alsó/keret), button 1..8 (B1..B8)
inline uint8_t mullerInputOf(uint8_t layer, uint8_t button) {
  static const uint8_t btnToIdx[9] = {0xFF, 0, 1, 2, 4, 3, 5, 6, 7};
  if (layer > 2 || button < 1 || button > 8) return 0xFF;
  return (uint8_t)(layer * 8 + btnToIdx[button]);
}

inline const char* mullerLayerName(uint8_t layer, bool en = true) {
  static const char* e[3] = {"TRAIL", "SPRAY", "BOOM"};
  static const char* h[3] = {"TRAIL", "Permetezés", "Keret"};
  return layer < 3 ? (en ? e[layer] : h[layer]) : "?";
}
