#pragma once
// Müller (ME) permetező joystick funkciótábla - a kijelzőhöz (ikon + felirat).
// Gombszám = váltóállás * 8 + belső index; belső index: B1=0 B2=1 B3=2 B5=3 B4=4 B6=5 B7=6 B8=7
// Forrás: docs/muller/ (bemért), + a felhasználó gépén a TRAIL B3 = vonórúd középre (tartásos).
#include <stdint.h>

enum class MIcon : uint8_t {
  None,
  Target100, SectMain, SectOnRL, SectOnLR, SectOffLR, SectOffRL, RateMinus, RatePlus,
  BoomAuto, BoomUp, BoomDown, FoldIn, FoldOut, TiltLeftUp, TiltRightUp,
  TrailAuto, TrailOnOff, TrailCenter, SteerLeft, SteerRight,
};

struct MFunc {
  MIcon       icon;
  const char* label;   // rövid magyar felirat (üres = nincs kiosztva)
  bool        hold;    // tartásos (mozgató) funkció - csak kijelzéshez
};

inline const MFunc& mullerFunc(uint8_t input) {
  static const MFunc T[25] = {
    /* 0  TRAIL B1 */ {MIcon::TrailAuto,   "Kézi/auto",   false},
    /* 1  TRAIL B2 */ {MIcon::TrailOnOff,  "TRAIL be/ki", false},
    /* 2  TRAIL B3 */ {MIcon::TrailCenter, "Középre",     true},
    /* 3  TRAIL B5 */ {MIcon::None,        "",            false},
    /* 4  TRAIL B4 */ {MIcon::None,        "",            false},
    /* 5  TRAIL B6 */ {MIcon::None,        "",            false},
    /* 6  TRAIL B7 */ {MIcon::SteerLeft,   "Balra",       true},
    /* 7  TRAIL B8 */ {MIcon::SteerRight,  "Jobbra",      true},
    /* 8  PERM  B1 */ {MIcon::Target100,   "Célérték",    false},
    /* 9  PERM  B2 */ {MIcon::SectMain,    "Főkapcsoló",  false},
    /* 10 PERM  B3 */ {MIcon::SectOnRL,    "Be jobbról",  false},
    /* 11 PERM  B5 */ {MIcon::SectOffLR,   "Ki balról",   false},
    /* 12 PERM  B4 */ {MIcon::SectOnLR,    "Be balról",   false},
    /* 13 PERM  B6 */ {MIcon::SectOffRL,   "Ki jobbról",  false},
    /* 14 PERM  B7 */ {MIcon::RateMinus,   "Adag -",      false},
    /* 15 PERM  B8 */ {MIcon::RatePlus,    "Adag +",      false},
    /* 16 KERET B1 */ {MIcon::BoomAuto,    "Kézi/auto",   false},
    /* 17 KERET B2 */ {MIcon::None,        "",            false},
    /* 18 KERET B3 */ {MIcon::BoomUp,      "Emelés",      true},
    /* 19 KERET B5 */ {MIcon::BoomDown,    "Süllyesztés", true},
    /* 20 KERET B4 */ {MIcon::FoldIn,      "Behajtás",    true},
    /* 21 KERET B6 */ {MIcon::FoldOut,     "Kihajtás",    true},
    /* 22 KERET B7 */ {MIcon::TiltLeftUp,  "Bal fel",     true},
    /* 23 KERET B8 */ {MIcon::TiltRightUp, "Jobb fel",    true},
    /* --        */ {MIcon::None,        "",            false},
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

inline const char* mullerLayerName(uint8_t layer) {
  static const char* n[3] = {"TRAIL", "Permetezés", "Keret"};
  return layer < 3 ? n[layer] : "?";
}
