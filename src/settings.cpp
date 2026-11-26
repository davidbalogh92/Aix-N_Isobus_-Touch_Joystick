#include "settings.h"
#include <Preferences.h>
#include "muller_layout.h"

Settings g_set;

static const char* NS = "auxjoy";

void settingsLoad() {
  Preferences p;
  p.begin(NS, true);
  g_set.layout      = p.getUChar("layout", 6);
  g_set.panelSt7789 = p.getBool("st7789", false);
  g_set.invert      = p.getBool("invert", false);
  g_set.bgr         = p.getBool("bgr", false);
  g_set.brightness  = p.getUChar("bright", 192);
  g_set.beep        = p.getBool("beep", true);
  g_set.rotation    = p.getUChar("rot", 1) & 3;
  g_set.mode        = p.getUChar("mode", MODE_AUXN);
  g_set.mullerLayer = p.getUChar("mlayer", 1);
  if (g_set.mode > MODE_MULLER) g_set.mode = MODE_AUXN;
  if (g_set.mullerLayer > 2) g_set.mullerLayer = 1;
  g_set.lang        = p.getUChar("lang", LANG_EN) ? LANG_HU : LANG_EN;
  g_set.autoBright  = p.getBool("abright", false);
  g_set.lightCh     = p.getUChar("lch", 0xFF);
  g_set.lightDark   = p.getUShort("ldark", 0);
  g_set.lightBright = p.getUShort("lbright", 0);
  if (g_set.lightCh > 3) g_set.lightCh = 0xFF;
  // Müller kiosztás: új formátum (mslots), vagy átvétel a v0.5-0.6 gombonkénti beállításból (mcfg)
  if (p.getBytes("mslots", g_set.mSlots, sizeof(g_set.mSlots)) == sizeof(g_set.mSlots) &&
      p.getBytes("mtabs", g_set.mTabNames, sizeof(g_set.mTabNames)) == sizeof(g_set.mTabNames)) {
    // megvan
  } else {
    uint8_t legacy[24];
    layoutDefault(g_set.mSlots, g_set.mTabNames);
    if (p.getBytes("mcfg", legacy, sizeof(legacy)) == sizeof(legacy)) layoutFromLegacy(legacy, g_set.mSlots);
  }
  for (uint8_t t = 0; t < 3; t++) {
    if (g_set.mTabNames[t] >= TAB_NAMES) g_set.mTabNames[t] = t;
    for (uint8_t b = 0; b < 8; b++) {
      uint8_t c = g_set.mSlots[t][b];
      if (c != SLOT_EMPTY && ((c & 0x1F) > 23 || (c & 0x60))) g_set.mSlots[t][b] = SLOT_EMPTY;
    }
  }
  g_set.calValid    = p.getBool("calok", false);
  if (g_set.calValid) {
    if (p.getBytes("cal", g_set.cal, sizeof(g_set.cal)) != sizeof(g_set.cal)) {
      g_set.calValid = false;
    }
  }
  p.end();
  if (g_set.layout != 3 && g_set.layout != 6 && g_set.layout != 8) g_set.layout = 6;
  if (g_set.brightness < 32) g_set.brightness = 32;
}

void settingsSave() {
  Preferences p;
  p.begin(NS, false);
  p.putUChar("layout", g_set.layout);
  p.putBool("st7789", g_set.panelSt7789);
  p.putBool("invert", g_set.invert);
  p.putBool("bgr", g_set.bgr);
  p.putUChar("bright", g_set.brightness);
  p.putBool("beep", g_set.beep);
  p.putUChar("rot", g_set.rotation);
  p.putUChar("mode", g_set.mode);
  p.putUChar("mlayer", g_set.mullerLayer);
  p.putUChar("lang", g_set.lang);
  p.putBool("abright", g_set.autoBright);
  p.putUChar("lch", g_set.lightCh);
  p.putUShort("ldark", g_set.lightDark);
  p.putUShort("lbright", g_set.lightBright);
  p.putBytes("mslots", g_set.mSlots, sizeof(g_set.mSlots));
  p.putBytes("mtabs", g_set.mTabNames, sizeof(g_set.mTabNames));
  p.putBool("calok", g_set.calValid);
  p.putBytes("cal", g_set.cal, sizeof(g_set.cal));
  p.end();
}

uint8_t nextLayout(uint8_t cur) {
  if (cur == 3) return 6;
  if (cur == 6) return 8;
  return 3;
}

uint8_t nextRotation(uint8_t cur) {
  switch (cur & 3) {
    case 1: return 0;   // fekvő -> álló
    case 0: return 3;   // álló -> fekvő 180°
    case 3: return 2;   // fekvő 180° -> álló 180°
    default: return 1;  // álló 180° -> fekvő
  }
}
