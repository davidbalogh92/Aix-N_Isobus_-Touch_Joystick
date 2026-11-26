#include "settings.h"
#include <Preferences.h>
#include "muller_profile.h"

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
  g_set.mProfile    = p.getUChar("mprof", PROF_TOWED_OWN);
  if (p.getBytes("mcfg", g_set.mCfg, sizeof(g_set.mCfg)) != sizeof(g_set.mCfg)) {
    if (g_set.mProfile >= PROF_CUSTOM) g_set.mProfile = PROF_TOWED_OWN;
    mullerApplyProfile(g_set.mProfile, g_set.mCfg);
  }
  for (uint8_t i = 0; i < 24; i++) if (g_set.mCfg[i] > BTN_ALT) g_set.mCfg[i] = BTN_ON;
  g_set.mProfile = mullerProfileOf(g_set.mCfg);
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
  p.putUChar("mprof", g_set.mProfile);
  p.putBytes("mcfg", g_set.mCfg, sizeof(g_set.mCfg));
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
