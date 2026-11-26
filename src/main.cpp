// =============================================================
//  AUX-N Touch Joystick
//  - kijelző + önteszt (színsávok, LED, hang), érintés kalibrálás (NVS)
//  - AUX-N mód: 3 / 6 / 8 gombos elrendezés (BOOT rövid nyomás váltja) - ISOBUS réteg később
//  - Müller (ME) mód: JB3 stílusú képernyő, 3 színes fül (TRAIL / SPRAY / BOOM) + 8 gomb a joystick
//    formájában, AUX-O küldés (docs/TERV_muller_mod.md), gépprofilok + gombszerkesztő
//  - nyelv: English / Magyar (kijelző); a soros log magyar
//  - CAN figyelés (250k), CAN diagnosztika
//  - beállítások (BOOT 2 mp), 2 oldal
//  Készítette: Balogh Dávid & Claude
// =============================================================
#include <Arduino.h>
#include "config.h"
#include "lgfx_board.h"
#include "fonts_hu.h"
#include "settings.h"
#include "feedback.h"
#include "can_monitor.h"
#include "keys.h"
#include "muller_out.h"
#include "muller_funcs.h"
#include "muller_profile.h"
#include "muller_icons.h"
#include "light_logic.h"

#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

static constexpr uint16_t C_BG       = RGB565(16, 18, 22);
static constexpr uint16_t C_BAR      = RGB565(36, 40, 48);
static constexpr uint16_t C_KEY      = RGB565(52, 58, 70);
static constexpr uint16_t C_KEY_EDGE = RGB565(110, 120, 140);
static constexpr uint16_t C_KEY_ON   = RGB565(255, 150, 0);
static constexpr uint16_t C_TEXT     = RGB565(235, 235, 235);
static constexpr uint16_t C_DIM      = RGB565(150, 155, 165);
static constexpr uint16_t C_OK       = RGB565(60, 200, 90);
static constexpr uint16_t C_WARN     = RGB565(240, 200, 40);
static constexpr uint16_t C_ERR      = RGB565(240, 60, 50);
static constexpr uint16_t C_BLACK    = RGB565(0, 0, 0);
static constexpr uint16_t C_WHITE    = RGB565(245, 245, 245);
// Müller mód - JB3 stílus
static constexpr uint16_t C_MKEY      = RGB565(74, 76, 80);
static constexpr uint16_t C_MKEY_EDGE = RGB565(120, 124, 130);
static constexpr uint16_t C_MFREE     = RGB565(44, 46, 50);
static constexpr uint16_t C_IBLUE     = RGB565(30, 140, 235);
static constexpr uint16_t C_IGREEN    = RGB565(80, 210, 70);
static constexpr uint16_t C_IAUTO     = RGB565(60, 170, 60);
static constexpr uint16_t C_LAYER_ON[3]  = {RGB565(230, 40, 40), RGB565(245, 200, 30), RGB565(70, 190, 70)};
static constexpr uint16_t C_LAYER_DIM[3] = {RGB565(90, 30, 30), RGB565(95, 80, 20), RGB565(30, 80, 35)};

static LGFX lcd;
static inline int16_t SW() { return (int16_t)lcd.width(); }
static inline int16_t SH() { return (int16_t)lcd.height(); }

#if defined(BOARD_E32R35T)
static const bool PANEL_SELECTABLE = false;
#else
static const bool PANEL_SELECTABLE = true;
#endif

// ---------------- nyelv ----------------
static inline bool isHu() { return g_set.lang == LANG_HU; }
static inline bool isEn() { return g_set.lang != LANG_HU; }
static inline const char* tr(const char* en, const char* hu) { return isHu() ? hu : en; }

// ---------------- font kezelés ----------------
static const uint8_t* s_font = nullptr;
static void useFont(const uint8_t* f) {
  if (f == s_font) return;
  lcd.unloadFont();
  lcd.loadFont(f);
  s_font = f;
}
static void forgetFont() {   // pl. calibrateTouch után
  lcd.unloadFont();
  s_font = nullptr;
}

// ---------------- állapot ----------------
enum class Screen : uint8_t { Main, Settings, Editor };
static Screen s_screen = Screen::Main;

static KeyRect s_keys[MAX_KEYS];
static uint8_t s_keyCount = 0;
static int8_t  s_activeKey = -1;
bool g_keyDown[MAX_KEYS] = {false};   // később ezt olvassa az AUX-N réteg

// Müller
static MullerAuxOSender s_muller;
static KeyRect s_tabs[3];
static uint8_t s_tabLayer[3];          // látható fül index -> réteg
static uint8_t s_tabCount = 0;
static uint8_t s_editLayer = 0;        // szerkesztőben kijelölt fül
static KeyRect s_editDone;             // szerkesztő "Kész" gomb
static inline bool isMuller() { return g_set.mode == MODE_MULLER; }

static TouchFilter s_touch({TOUCH_PRESS_STABLE_MS, TOUCH_RELEASE_MS,
                            TOUCH_STABLE_RADIUS, TOUCH_JUMP_RADIUS});

static char     s_barCache[96] = "";
static uint32_t s_lastBarMs = 0;
static uint32_t s_lastTouchMs = 0;

// beállítások: 2 oldal, azonosítók
enum SetId : uint8_t {
  S_CAL, S_ROT, S_BRIGHT, S_BEEP, S_LANG, S_INV, S_BGR, S_PANEL, S_MORE, S_EXIT,
  S_MODE, S_PROFILE, S_EDIT, S_KEYS, S_CANINFO, S_LIGHT, S_BACK,
};
static const SetId SET_PAGE0[] = {S_CAL, S_ROT, S_BRIGHT, S_BEEP, S_LANG, S_INV, S_BGR, S_PANEL, S_MORE, S_EXIT};
static const SetId SET_PAGE1[] = {S_MODE, S_PROFILE, S_EDIT, S_KEYS, S_CANINFO, S_LIGHT, S_BACK};
static uint8_t s_setPage = 0;
static KeyRect s_setRects[10];
static uint8_t s_setCount = 0;
static int8_t  s_setPressed = -1;     // beállítások / szerkesztő: lenyomott elem

// BOOT gomb
static bool     s_bootDown = false;
static uint32_t s_bootT0 = 0;
static bool     s_bootLongFired = false;

// =============================================================
//  Rajzolás - segédek
// =============================================================
static void drawCentered(const char* txt, int16_t x, int16_t y, const uint8_t* font,
                         uint16_t fg, uint16_t bg) {
  useFont(font);
  lcd.setTextColor(fg, bg);
  lcd.setTextDatum(lgfx::textdatum::middle_center);
  lcd.drawString(txt, x, y);
}

// Középre írás: ha a 16 px-es nem fér el maxW-ben, 13 px-es fonttal
static void drawFit(const char* txt, int16_t x, int16_t y, int16_t maxW, uint16_t fg, uint16_t bg) {
  useFont(font_hu16);
  const uint8_t* f = (lcd.textWidth(txt) <= maxW) ? font_hu16 : font_hu13;
  drawCentered(txt, x, y, f, fg, bg);
}

// Kredit: ha keskeny a kijelző (álló), két sorban
static void drawCredit(int16_t yCenter, uint16_t fg) {
  if (SW() >= 340) {
    drawCentered(FW_CREDIT, SW() / 2, yCenter, font_hu16, fg, C_BG);
  } else {
    drawCentered("Készítette:", SW() / 2, yCenter - 18, font_hu16, fg, C_BG);
    drawCentered("Balogh Dávid & Claude", SW() / 2, yCenter, font_hu16, fg, C_BG);
  }
}

static const char* rotationName(uint8_t r) {
  switch (r & 3) {
    case 0:  return tr("Portrait", "Álló");
    case 1:  return tr("Landscape", "Fekvő");
    case 2:  return tr("Portrait 180°", "Álló 180°");
    default: return tr("Landsc. 180°", "Fekvő 180°");
  }
}

static void drawHeader(const char* title, const char* right) {
  lcd.fillRect(0, 0, SW(), STATUS_H, C_BAR);
  useFont(font_hu16);
  lcd.setTextColor(C_TEXT, C_BAR);
  lcd.setTextDatum(lgfx::textdatum::middle_left);
  lcd.drawString(title, 6, STATUS_H / 2);
  if (right) {
    lcd.setTextColor(C_DIM, C_BAR);
    lcd.setTextDatum(lgfx::textdatum::middle_right);
    lcd.drawString(right, SW() - 6, STATUS_H / 2);
  }
}

static void drawStatusBar(bool force) {
  const CanStats& cs = canStats();
  char left[24], mid[48], right[32];
  if (isMuller()) snprintf(left, sizeof(left), "Müller");
  else            snprintf(left, sizeof(left), tr("%u keys", "%u gomb"), g_set.layout);
  if (cs.state == CanState::Ok && cs.vtSeen) snprintf(mid, sizeof(mid), "CAN: OK, VT:%u", cs.vtAddr);
  else                                         snprintf(mid, sizeof(mid), "%s", canStateText(cs.state, isHu()));
  if (isMuller()) {
    switch (s_muller.state()) {
      case MullerAuxOSender::State::Claimed:
        snprintf(right, sizeof(right), tr("addr 0x%02X", "cím 0x%02X"), s_muller.address()); break;
      case MullerAuxOSender::State::Claiming:    snprintf(right, sizeof(right), "%s", tr("claiming", "címfoglalás")); break;
      case MullerAuxOSender::State::CannotClaim: snprintf(right, sizeof(right), "%s", tr("no address", "nincs cím")); break;
      default:                                   snprintf(right, sizeof(right), "%s", tr("off", "ki")); break;
    }
  } else if (cs.state == CanState::Ok && SW() >= 300) {
    snprintf(right, sizeof(right), tr("%u nodes", "%u eszköz"), cs.nodes);
  } else {
    right[0] = 0;
  }

  char all[110];
  snprintf(all, sizeof(all), "%s|%s|%s", left, mid, right);
  if (!force && strcmp(all, s_barCache) == 0) return;
  strncpy(s_barCache, all, sizeof(s_barCache) - 1);

  uint16_t stc = (cs.state == CanState::Ok) ? C_OK : (cs.state == CanState::NoTraffic ? C_WARN : C_ERR);
  lcd.fillRect(0, 0, SW(), STATUS_H, C_BAR);
  useFont(font_hu16);
  lcd.setTextColor(C_TEXT, C_BAR);
  lcd.setTextDatum(lgfx::textdatum::middle_left);
  lcd.drawString(left, 6, STATUS_H / 2);
  lcd.setTextColor(stc, C_BAR);
  lcd.drawString(mid, 80, STATUS_H / 2);
  lcd.setTextColor(isMuller() ? (s_muller.claimed() ? C_OK : C_WARN) : C_DIM, C_BAR);
  lcd.setTextDatum(lgfx::textdatum::middle_right);
  lcd.drawString(right, SW() - 4, STATUS_H / 2);
}

// =============================================================
//  AUX-N képernyő (sima gombrács)
// =============================================================
static void drawAuxKey(uint8_t i, bool on) {
  const KeyRect& k = s_keys[i];
  uint16_t fill = on ? C_KEY_ON : C_KEY;
  lcd.fillRoundRect(k.x, k.y, k.w, k.h, 10, fill);
  lcd.drawRoundRect(k.x, k.y, k.w, k.h, 10, on ? C_TEXT : C_KEY_EDGE);
  char num[4];
  snprintf(num, sizeof(num), "%u", i + 1);
  drawCentered(num, k.x + k.w / 2, k.y + k.h / 2, font_hu40, on ? C_BLACK : C_TEXT, fill);
}

// =============================================================
//  Müller képernyő (JB3 stílus)
// =============================================================
struct LcdIconGfx {
  void wideLine(float x0, float y0, float x1, float y1, float r, uint16_t c) { lcd.drawWideLine(x0, y0, x1, y1, r, c); }
  void fillTri(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) { lcd.fillTriangle(x0, y0, x1, y1, x2, y2, c); }
  void fillRect(int x, int y, int w, int h, uint16_t c) { lcd.fillRect(x, y, w, h, c); }
  void ring(int x, int y, int r, float th, uint16_t c) {
    int t = (int)(th + 0.5f);
    if (t < 1) t = 1;
    for (int rr = r - t; rr <= r + t; rr++) if (rr > 0) lcd.drawCircle(x, y, rr, c);
  }
  void text(const char* str, int cx, int cy, float u, uint16_t fg, uint16_t bg) {
    drawCentered(str, cx, cy, u < 7 ? font_hu13 : font_hu16, fg, bg);
  }
};

static int16_t tabH() { return SH() >= 300 ? 46 : 38; }

// Fülek: a látható rétegek (szerkesztőben mind a 3), a kijelölt teljes színnel
static void drawTabs(bool all, uint8_t selected) {
  lcd.fillRect(0, STATUS_H, SW(), tabH(), C_BG);
  s_tabCount = 0;
  for (uint8_t l = 0; l < 3; l++)
    if (all || mullerLayerVisible(g_set.mCfg, l)) s_tabLayer[s_tabCount++] = l;
  if (s_tabCount == 0) return;
  const int16_t gap = 6;
  const int16_t tw = (SW() - (s_tabCount + 1) * gap) / s_tabCount;
  for (uint8_t i = 0; i < s_tabCount; i++) {
    uint8_t l = s_tabLayer[i];
    s_tabs[i] = {(int16_t)(gap + i * (tw + gap)), (int16_t)(STATUS_H + 4), tw, (int16_t)(tabH() - 8)};
    bool on = (l == selected);
    uint16_t c = on ? C_LAYER_ON[l] : C_LAYER_DIM[l];
    lcd.fillRoundRect(s_tabs[i].x, s_tabs[i].y, s_tabs[i].w, s_tabs[i].h, 8, c);
    drawCentered(mullerLayerName(l, isEn()), s_tabs[i].x + tw / 2, s_tabs[i].y + s_tabs[i].h / 2,
                 tw < 130 ? font_hu13 : font_hu16, on ? (l == 1 ? C_BLACK : C_WHITE) : C_DIM, c);
  }
}

// Színes pont a B3..B6 blokk közepén (mint a joystick LED-je)
static void drawLayerDot(uint8_t layer) {
  int16_t dx = (s_keys[2].x + s_keys[2].w + s_keys[3].x) / 2;
  int16_t dy = (s_keys[2].y + s_keys[2].h + s_keys[4].y) / 2;
  lcd.fillCircle(dx, dy, 7, layer < 3 ? C_LAYER_ON[layer] : C_DIM);
}

// Egy Müller gomb. editor = szerkesztő nézet (a kikapcsolt gomb halványan + "OFF/KI")
static void drawMullerKeyEx(uint8_t b, uint8_t layer, bool on, bool editor) {
  const KeyRect& r = s_keys[b];
  uint8_t in = mullerInputOf(layer, b + 1);
  bool assigned = mullerAssigned(in);
  bool active = mullerBtnActive(g_set.mCfg, in);
  bool showIcon = active || (editor && assigned);
  uint16_t fill = !active ? C_MFREE : (on ? C_KEY_ON : C_MKEY);
  lcd.fillRoundRect(r.x, r.y, r.w, r.h, 10, active ? C_MKEY_EDGE : C_MFREE);
  lcd.fillRoundRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, 9, fill);
  if (editor && on) lcd.drawRoundRect(r.x, r.y, r.w, r.h, 10, C_KEY_ON);

  bool label = showIcon && r.h >= 80;
  int iconH = r.h - (label ? 18 : 0);
  if (showIcon) {
    MIconColors k = active ? MIconColors{C_WHITE, C_IBLUE, C_IGREEN, fill, C_IAUTO}
                           : MIconColors{RGB565(95, 98, 104), RGB565(60, 80, 110), RGB565(70, 100, 70), fill,
                                         RGB565(50, 70, 50)};
    int sz = (int)((r.w < iconH ? r.w : iconH) * 0.74f);
    LcdIconGfx g;
    drawMullerIcon(g, mullerIconFor(g_set.mCfg, in), r.x + r.w / 2, r.y + iconH / 2 + (label ? 3 : 0), sz, k);
  }
  if (label) {
    const char* txt = (editor && !active) ? tr("OFF", "KI") : mullerLabelFor(g_set.mCfg, in, isEn());
    drawFit(txt, r.x + r.w / 2, r.y + r.h - 11, r.w - 6, (on && active) ? C_BLACK : (active ? C_TEXT : C_DIM), fill);
  } else if (editor && !active && assigned) {
    drawCentered(tr("OFF", "KI"), r.x + r.w / 2, r.y + r.h / 2, font_hu13, C_DIM, fill);
  }
  char nb[4];
  snprintf(nb, sizeof(nb), "B%u", b + 1);
  drawCentered(nb, r.x + 14, r.y + 11, font_hu13, active ? RGB565(170, 172, 178) : RGB565(90, 92, 98), fill);
}

static void drawMullerKey(uint8_t b, bool on) { drawMullerKeyEx(b, g_set.mullerLayer, on, false); }

static void drawKey(uint8_t i, bool on) {
  if (isMuller()) drawMullerKey(i, on);
  else            drawAuxKey(i, on);
}

// Ha az aktuális fül nem látható (pl. önjáró profil), az első láthatóra lépünk
static void fixMullerLayer() {
  if (!mullerLayerVisible(g_set.mCfg, g_set.mullerLayer)) {
    uint8_t f = mullerFirstVisible(g_set.mCfg);
    if (f != 0xFF) g_set.mullerLayer = f;
  }
}

static void drawMullerMain() {
  fixMullerLayer();
  drawTabs(false, g_set.mullerLayer);
  if (s_tabCount == 0) {
    drawCentered(tr("No buttons enabled", "Nincs bekapcsolt gomb"), SW() / 2, SH() / 2, font_hu16, C_WARN, C_BG);
    drawCentered(tr("Settings > Functions > Edit buttons", "Beállítások > Funkciók > Gombok"), SW() / 2,
                 SH() / 2 + 24, font_hu13, C_DIM, C_BG);
    s_keyCount = 0;
    return;
  }
  mullerLayout(0, STATUS_H + tabH(), SW(), SH() - STATUS_H - tabH(), s_keys);
  s_keyCount = 8;
  for (uint8_t i = 0; i < 8; i++) drawMullerKey(i, (int8_t)i == s_activeKey);
  drawLayerDot(g_set.mullerLayer);
}

static void drawMain() {
  lcd.fillScreen(C_BG);
  if (isMuller()) {
    drawMullerMain();
  } else {
    s_keyCount = layoutCompute(g_set.layout, 0, STATUS_H, SW(), SH() - STATUS_H, s_keys, MAX_KEYS);
    for (uint8_t i = 0; i < s_keyCount; i++) drawAuxKey(i, (int8_t)i == s_activeKey);
  }
  drawStatusBar(true);
}

// ---- gombszerkesztő ----
static void drawEditor() {
  lcd.fillScreen(C_BG);
  char right[24];
  snprintf(right, sizeof(right), "%s", mullerProfileName(g_set.mProfile, isEn()));
  drawHeader(tr("Edit buttons", "Gombok szerk."), nullptr);
  // "Kész" gomb a fejléc jobb oldalán
  int16_t dw = 84;
  s_editDone = {(int16_t)(SW() - dw - 4), 2, dw, (int16_t)(STATUS_H - 4)};
  lcd.fillRoundRect(s_editDone.x, s_editDone.y, s_editDone.w, s_editDone.h, 6, RGB565(40, 110, 60));
  drawCentered(tr("Done", "Kész"), s_editDone.x + dw / 2, s_editDone.y + s_editDone.h / 2, font_hu16, C_WHITE,
               RGB565(40, 110, 60));
  // profil név középen
  useFont(font_hu13);
  lcd.setTextColor(C_DIM, C_BAR);
  lcd.setTextDatum(lgfx::textdatum::middle_right);
  lcd.drawString(right, s_editDone.x - 8, STATUS_H / 2);

  drawTabs(true, s_editLayer);
  mullerLayout(0, STATUS_H + tabH(), SW(), SH() - STATUS_H - tabH(), s_keys);
  s_keyCount = 8;
  for (uint8_t i = 0; i < 8; i++) drawMullerKeyEx(i, s_editLayer, false, true);
  drawLayerDot(s_editLayer);
}

// =============================================================
//  Beállítások
// =============================================================
static SetId setIdAt(uint8_t i) { return s_setPage == 0 ? SET_PAGE0[i] : SET_PAGE1[i]; }

static void settingLabel(SetId id, char* buf, size_t n) {
  switch (id) {
    case S_CAL:     snprintf(buf, n, "%s", tr("Calibrate", "Kalibrálás")); break;
    case S_ROT:     snprintf(buf, n, "%s", rotationName(g_set.rotation)); break;
    case S_BRIGHT:
      if (g_set.autoBright)
        snprintf(buf, n, "%s%s", tr("Brightness Auto", "Fény: Auto"), g_set.lightCh == LIGHT_NONE ? " (!)" : "");
      else
        snprintf(buf, n, tr("Brightness %u%%", "Fény: %u%%"), (unsigned)((g_set.brightness * 100 + 127) / 255));
      break;
    case S_BEEP:    snprintf(buf, n, "%s %s", tr("Sound:", "Hang:"), g_set.beep ? tr("On", "Be") : tr("Off", "Ki")); break;
    case S_LANG:    snprintf(buf, n, "%s", tr("Language: English", "Nyelv: Magyar")); break;
    case S_INV:     snprintf(buf, n, "Invert: %s", g_set.invert ? tr("On", "Be") : tr("Off", "Ki")); break;
    case S_BGR:     snprintf(buf, n, "%s %s", tr("Colors:", "Szín:"), g_set.bgr ? "BGR" : "RGB"); break;
#if defined(BOARD_E32R35T)
    case S_PANEL:   snprintf(buf, n, "Panel: ST7796"); break;
#else
    case S_PANEL:   snprintf(buf, n, "Panel: %s", g_set.panelSt7789 ? "ST7789" : "ILI9341"); break;
#endif
    case S_MORE:    snprintf(buf, n, "%s", tr("Functions >>", "Funkciók >>")); break;
    case S_EXIT:    snprintf(buf, n, "%s", tr("Exit", "Kilépés")); break;
    case S_MODE:    snprintf(buf, n, "%s %s", tr("Mode:", "Mód:"), isMuller() ? "Müller" : "AUX-N"); break;
    case S_PROFILE: snprintf(buf, n, "%s %s", tr("Machine:", "Gép:"), mullerProfileName(g_set.mProfile, isEn())); break;
    case S_EDIT:    snprintf(buf, n, "%s", tr("Edit buttons", "Gombok szerk.")); break;
    case S_KEYS:    snprintf(buf, n, tr("AUX-N keys: %u", "AUX-N gomb: %u"), g_set.layout); break;
    case S_CANINFO: snprintf(buf, n, "CAN info"); break;
    case S_LIGHT:   snprintf(buf, n, "%s", tr("Light sensor", "Fényérzékelő")); break;
    case S_BACK:    snprintf(buf, n, "%s", tr("<< Back", "<< Vissza")); break;
  }
}

static void drawSettingItem(uint8_t i, bool on) {
  SetId id = setIdAt(i);
  const KeyRect& k = s_setRects[i];
  uint16_t base = (id == S_EXIT) ? RGB565(40, 90, 60) : ((id == S_MORE || id == S_BACK) ? RGB565(45, 65, 100) : C_KEY);
  uint16_t fill = on ? C_KEY_ON : base;
  lcd.fillRoundRect(k.x, k.y, k.w, k.h, 8, fill);
  lcd.drawRoundRect(k.x, k.y, k.w, k.h, 8, C_KEY_EDGE);
  char buf[40];
  settingLabel(id, buf, sizeof(buf));
  drawFit(buf, k.x + k.w / 2, k.y + k.h / 2, k.w - 8, on ? C_BLACK : C_TEXT, fill);
}

static void drawSettings() {
  lcd.fillScreen(C_BG);
  char title[40];
  snprintf(title, sizeof(title), "%s %u/2", tr("Settings", "Beállítások"), s_setPage + 1);
  drawHeader(title, "v" FW_VERSION);
  s_setCount = (s_setPage == 0) ? sizeof(SET_PAGE0) / sizeof(SET_PAGE0[0]) : sizeof(SET_PAGE1) / sizeof(SET_PAGE1[0]);
  const int16_t top = STATUS_H + 6, footer = (SW() >= 340) ? 22 : 40, gap = 6;
  const int16_t w = (SW() - 3 * gap) / 2;
  const int16_t rows = (s_setCount + 1) / 2;
  const int16_t h = (SH() - top - footer - rows * gap) / rows;
  for (uint8_t i = 0; i < s_setCount; i++) {
    s_setRects[i] = {(int16_t)(gap + (i % 2) * (w + gap)), (int16_t)(top + (i / 2) * (h + gap)), w, h};
    drawSettingItem(i, false);
  }
  drawCredit(SH() - 12, C_DIM);
}

// =============================================================
//  Önteszt, nyitókép, kalibrálás
// =============================================================
static void selfTest() {
  const char* names[4] = {tr("RED", "PIROS"), tr("GREEN", "ZÖLD"), tr("BLUE", "KÉK"), tr("WHITE", "FEHÉR")};
  const uint16_t cols[4] = {RGB565(255, 0, 0), RGB565(0, 255, 0), RGB565(0, 0, 255), RGB565(255, 255, 255)};
  const int16_t w = SW() / 4;
  for (int i = 0; i < 4; i++) {
    lcd.fillRect(i * w, 0, w, SH(), cols[i]);
    drawCentered(names[i], i * w + w / 2, SH() / 2, font_hu16, i == 3 ? C_BLACK : RGB565(255, 255, 255), cols[i]);
  }
  fbLedRaw(true, false, false);  delay(250);
  fbLedRaw(false, true, false);  delay(250);
  fbLedRaw(false, false, true);  delay(250);
  fbLedRaw(false, false, false);
  fbBeep(1500, 80);
  uint32_t t = millis();
  while (millis() - t < 500) { fbLoop(); delay(5); }
}

static void doCalibrate() {
  lcd.fillScreen(C_BG);
  drawCentered(tr("Touch calibration", "Érintés kalibrálás"), SW() / 2, SH() / 2 - 30, font_hu22, C_TEXT, C_BG);
  drawCentered(tr("Touch the arrow tips", "Érintsd meg sorban"), SW() / 2, SH() / 2 + 10, font_hu16, C_DIM, C_BG);
  drawCentered(tr("one after another", "a nyilak hegyét"), SW() / 2, SH() / 2 + 32, font_hu16, C_DIM, C_BG);
  delay(1500);
  forgetFont();
  const uint16_t fg = RGB565(255, 255, 255), bg = C_BG;
  lcd.calibrateTouch(g_set.cal, fg, bg, 20);
  lcd.setTouchCalibrate(g_set.cal);
  g_set.calValid = true;
  settingsSave();
  Serial.println("[TOUCH] kalibráció mentve");
  fbBeep(2000, 60);
  int32_t x, y;
  while (lcd.getTouch(&x, &y)) delay(10);   // várjuk meg, hogy felengedje
  delay(200);
}

static void blinkPanelCode(bool st7789) {
  // 1 villanás = ILI9341, 2 villanás = ST7789 (hátha a kijelzőn nem látszik semmi)
  int n = st7789 ? 2 : 1;
  for (int i = 0; i < n; i++) {
    fbLedRaw(true, true, true);  delay(250);
    fbLedRaw(false, false, false); delay(250);
  }
}

static void splash() {
  lcd.fillScreen(C_BG);
  const int16_t cy = SH() / 2;
  drawCentered("AUX-N", SW() / 2, cy - 72, font_hu40, C_KEY_ON, C_BG);
  drawCentered("Touch Joystick", SW() / 2, cy - 32, font_hu22, C_TEXT, C_BG);
  drawCentered("v" FW_VERSION "  " BOARD_NAME, SW() / 2, cy - 6, font_hu16, C_DIM, C_BG);
  drawCentered(tr("Touch 1 s: calibrate", "Érintés 1 mp: kalibrálás"), SW() / 2, cy + 30, font_hu16, C_DIM, C_BG);
#if !defined(BOARD_E32R35T)
  drawCentered(tr("BOOT 2 s: panel type", "BOOT 2 mp: panel váltás"), SW() / 2, cy + 52, font_hu16, C_DIM, C_BG);
#endif
  drawCredit(SH() - 24, C_TEXT);

  uint32_t t0 = millis(), touchT0 = 0, bootT0 = 0;
  bool wantCal = false;
  while (millis() - t0 < 3000) {
    int32_t x, y;
    bool touched = lcd.getTouch(&x, &y);
    if (touched) {
      if (!touchT0) touchT0 = millis();
      if (millis() - touchT0 >= 1000) { wantCal = true; break; }
    } else {
      touchT0 = 0;
    }
    if (!PANEL_SELECTABLE) {
      // 3.5"-on nincs panelválasztás
    } else if (digitalRead(PIN_BOOT) == LOW) {
      if (!bootT0) bootT0 = millis();
      if (millis() - bootT0 >= 2000) {
        g_set.panelSt7789 = !g_set.panelSt7789;
        g_set.invert = g_set.panelSt7789;   // ST7789 általában invertált
        settingsSave();
        Serial.printf("[LCD] panel -> %s, újraindítás\n", g_set.panelSt7789 ? "ST7789" : "ILI9341");
        blinkPanelCode(g_set.panelSt7789);
        ESP.restart();
      }
    } else {
      bootT0 = 0;
    }
    delay(10);
  }
  if (wantCal || !g_set.calValid) doCalibrate();
}

// =============================================================
//  Logika
// =============================================================
// Az aktív gomb váltása. Müller módban k = B1..B8 indexe (0..7); a küldést is itt kezeljük,
// így minden elengedési út (felengedés, ugrás, képernyőváltás, hiba) ss=0-t küld.
static void setActiveKey(int8_t k) {
  if (k == s_activeKey) return;
  int8_t old = s_activeKey;
  s_activeKey = k;
  uint32_t now = millis();
  if (old >= 0) {
    if (isMuller()) {
      s_muller.release(now);
      Serial.printf("[ME] B%d FEL\n", old + 1);
    } else {
      g_keyDown[old] = false;
      Serial.printf("[KEY] %d FEL\n", old + 1);
    }
    if (s_screen == Screen::Main && old < s_keyCount) drawKey(old, false);
  }
  if (k >= 0) {
    if (isMuller()) {
      uint8_t in = mullerInputOf(g_set.mullerLayer, k + 1);
      bool sent = s_muller.press(in, now);
      Serial.printf("[ME] %s B%d (gombszám %u, %s)%s\n", mullerLayerName(g_set.mullerLayer, false), k + 1, in,
                    mullerLabelFor(g_set.mCfg, in, false), sent ? "" : "  -> NINCS ELKÜLDVE (nincs cím)");
    } else {
      g_keyDown[k] = true;
      Serial.printf("[KEY] %d LE\n", k + 1);
    }
    if (s_screen == Screen::Main) drawKey(k, true);
    fbBeep(2400, 25);
  }
  fbKeyActive(k >= 0);
}

static void setMullerLayer(uint8_t layer) {
  if (layer > 2) return;
  setActiveKey(-1);
  g_set.mullerLayer = layer;
  settingsSave();
  Serial.printf("[ME] fül: %s\n", mullerLayerName(layer, false));
  drawTabs(false, layer);
  for (uint8_t i = 0; i < 8; i++) drawMullerKey(i, false);
  drawLayerDot(layer);
}

static void applyMode() {
  if (isMuller()) {
    if (s_muller.state() == MullerAuxOSender::State::Off) {
      s_muller.begin(MullerAuxOSender::DEFAULT_IDENTITY, MullerAuxOSender::DEFAULT_ADDRESS, millis());
      Serial.println("[ME] Müller mód BE, címfoglalás 0x90");
    }
  } else if (s_muller.state() != MullerAuxOSender::State::Off) {
    s_muller.stop(millis());
    Serial.println("[ME] Müller mód KI");
  }
}

static void enterScreen(Screen s) {
  setActiveKey(-1);
  s_setPressed = -1;
  s_screen = s;
  if (s == Screen::Main) drawMain();
  else if (s == Screen::Editor) drawEditor();
  else drawSettings();
}

// CAN diagnosztika képernyő: érintésre vagy BOOT-ra kilép
static void showCanInfo() {
  int32_t x, y;
  while (lcd.getTouch(&x, &y)) delay(10);   // előbb engedje fel
  lcd.fillScreen(C_BG);
  drawHeader(tr("CAN diagnostics", "CAN diagnosztika"), nullptr);
  drawCentered(tr("Touch: back", "Érintés: vissza"), SW() / 2, SH() - 14, font_hu16, C_DIM, C_BG);

  uint32_t last = 0;
  while (true) {
    canLoop();
    fbLoop();
    s_muller.update(millis());
    if (lcd.getTouch(&x, &y) || digitalRead(PIN_BOOT) == LOW) break;
    uint32_t now = millis();
    if (now - last < 300) { delay(5); continue; }
    last = now;
    const CanStats& cs = canStats();
    char l[8][44];
    snprintf(l[0], 44, "%s", canStateText(cs.state, isHu()));
    snprintf(l[1], 44, tr("Traffic: %lu frame/s", "Forgalom: %lu frame/s"), (unsigned long)cs.fps);
    snprintf(l[2], 44, tr("Nodes: %u", "Eszközök: %u"), cs.nodes);
    if (cs.vtSeen) snprintf(l[3], 44, tr("VT (terminal): addr %u", "VT (terminál): cím %u"), cs.vtAddr);
    else           snprintf(l[3], 44, "%s", tr("VT (terminal): not seen", "VT (terminál): nem látszik"));
    snprintf(l[4], 44, tr("Error count TX/RX: %lu / %lu", "Hibaszámláló TX/RX: %lu / %lu"),
             (unsigned long)cs.txErr, (unsigned long)cs.rxErr);
    snprintf(l[5], 44, tr("Total frames: %lu", "Összes frame: %lu"), (unsigned long)cs.totalRx);
    snprintf(l[6], 44, tr("Pins: TX=IO%d  RX=IO%d", "Lábak: TX=IO%d  RX=IO%d"), PIN_CAN_TX, PIN_CAN_RX);
    if (!isMuller())             snprintf(l[7], 44, "%s", tr("Müller mode: off", "Müller mód: ki"));
    else if (s_muller.claimed()) snprintf(l[7], 44, tr("Müller: addr 0x%02X", "Müller: cím 0x%02X"), s_muller.address());
    else                         snprintf(l[7], 44, "%s", tr("Müller: no address", "Müller: nincs cím"));
    useFont(font_hu16);
    lcd.setTextDatum(lgfx::textdatum::middle_left);
    for (int i = 0; i < 8; i++) {
      int16_t yy = STATUS_H + 18 + i * 24;
      lcd.fillRect(0, yy - 11, SW(), 22, C_BG);
      lcd.setTextColor(i == 0 ? (cs.state == CanState::Ok ? C_OK : C_WARN) : C_TEXT, C_BG);
      lcd.drawString(l[i], 8, yy);
    }
  }
  while (lcd.getTouch(&x, &y) || digitalRead(PIN_BOOT) == LOW) delay(10);
  delay(100);
}

// =============================================================
//  Fényérzékelő
// =============================================================
static bool     s_lightReset = true;
static uint32_t s_lightState = 0;
static uint32_t s_lastLightMs = 0;

// csatorna: 0 = IO34 11dB, 1 = IO34 0dB, 2 = IO39 11dB, 3 = IO39 0dB
static uint16_t lightRead(uint8_t ch) {
  uint8_t pin = (ch < 2) ? PIN_LIGHT_A : PIN_LIGHT_B;
  analogSetPinAttenuation(pin, (ch & 1) ? ADC_0db : ADC_11db);
  (void)analogRead(pin);                     // első minta eldobva (csillapítás váltás után)
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return (uint16_t)(sum / 8);
}

static const char* lightChName(uint8_t ch) {
  static const char* n[4] = {"IO34 11dB", "IO34 0dB", "IO39 11dB", "IO39 0dB"};
  return ch < 4 ? n[ch] : "-";
}

// Auto fényerő: 250 ms-onként, simítva
static void autoBrightLoop(uint32_t now) {
  if (!g_set.autoBright || g_set.lightCh == LIGHT_NONE) return;
  if (now - s_lastLightMs < 250) return;
  s_lastLightMs = now;
  uint16_t raw = lightRead(g_set.lightCh);
  uint16_t lvl = lightLevel(raw, g_set.lightDark, g_set.lightBright);
  uint8_t target = lightBrightness(lvl, AUTO_BRIGHT_MIN, AUTO_BRIGHT_MAX);
  static uint8_t applied = 0;
  uint16_t b = lightSmooth(s_lightState, target, s_lightReset);
  if (s_lightReset || (b > applied ? b - applied : applied - b) >= 3) {
    applied = (uint8_t)b;
    lcd.setBrightness(applied);
  }
  s_lightReset = false;
}

// Betanító / mérő képernyő
static void showLightSensor() {
  int32_t tx, ty;
  while (lcd.getTouch(&tx, &ty)) delay(10);
  lcd.fillScreen(C_BG);
  drawHeader(tr("Light sensor (R21)", "Fényérzékelő (R21)"), nullptr);

  const int16_t gap = 6, bh = (SH() >= 400) ? 56 : 44;
  const int16_t bw = (SW() - 4 * gap) / 3, by = SH() - bh - gap;
  KeyRect btn[3];
  const char* bl[3] = {tr("Dark", "Sötét"), tr("Bright", "Világos"), tr("Back", "Vissza")};
  const uint16_t bc[3] = {RGB565(40, 44, 60), RGB565(110, 100, 40), RGB565(40, 90, 60)};
  for (int i = 0; i < 3; i++) {
    btn[i] = {(int16_t)(gap + i * (bw + gap)), by, bw, bh};
    lcd.fillRoundRect(btn[i].x, btn[i].y, bw, bh, 8, bc[i]);
    drawFit(bl[i], btn[i].x + bw / 2, by + bh / 2, bw - 6, C_WHITE, bc[i]);
  }
  const int16_t infoY = by - 52;
  if (infoY > STATUS_H + 8 + 4 * 30 + 50) {   // kis kijelzőn (2.8" fekvő) nem fér el
    drawCentered(tr("Cover R21 -> Dark", "R21 letakarva -> Sötét"), SW() / 2, infoY, font_hu13, C_DIM, C_BG);
    drawCentered(tr("Lamp on R21 -> Bright", "Lámpa az R21-re -> Világos"), SW() / 2, infoY + 18, font_hu13, C_DIM, C_BG);
  }

  uint16_t dark[4] = {0}, bright[4] = {0};
  bool haveDark = false, haveBright = false;
  TouchFilter tf({TOUCH_PRESS_STABLE_MS, TOUCH_RELEASE_MS, TOUCH_STABLE_RADIUS, TOUCH_JUMP_RADIUS});
  uint32_t lastDraw = 0, lastTouch = 0;
  char res[64] = "";
  bool resDirty = true;

  while (true) {
    uint32_t now = millis();
    canLoop();
    fbLoop();
    s_muller.update(now);
    if (digitalRead(PIN_BOOT) == LOW) break;

    if (now - lastTouch >= TOUCH_SAMPLE_MS) {
      lastTouch = now;
      bool t = lcd.getTouch(&tx, &ty);
      if (tf.update(t, (int16_t)tx, (int16_t)ty, now) && !tf.pressed() && !tf.blocked()) {
        int8_t h = layoutHitTest(btn, 3, tf.x(), tf.y(), 2);
        if (h == 2) break;
        if (h == 0 || h == 1) {
          fbBeep(2000, 40);
          uint16_t* dst = (h == 0) ? dark : bright;
          for (uint8_t c = 0; c < 4; c++) dst[c] = lightRead(c);
          if (h == 0) haveDark = true; else haveBright = true;
          Serial.printf("[FÉNY] %s: %u %u %u %u\n", h == 0 ? "sötét" : "világos", dst[0], dst[1], dst[2], dst[3]);
          if (haveDark && haveBright) {
            uint8_t ch = lightPickChannel(dark, bright);
            if (ch == LIGHT_NONE) {
              snprintf(res, sizeof(res), "%s", tr("Not found - no change", "Nem található - nincs változás"));
            } else {
              g_set.lightCh = ch;
              g_set.lightDark = dark[ch];
              g_set.lightBright = bright[ch];
              settingsSave();
              s_lightReset = true;
              snprintf(res, sizeof(res), tr("Found: %s  OK", "Megvan: %s  OK"), lightChName(ch));
              Serial.printf("[FÉNY] csatorna: %s, sötét=%u világos=%u\n", lightChName(ch), dark[ch], bright[ch]);
            }
          } else {
            snprintf(res, sizeof(res), "%s", h == 0 ? tr("Dark saved, now Bright", "Sötét mentve, most Világos")
                                                    : tr("Bright saved, now Dark", "Világos mentve, most Sötét"));
          }
          resDirty = true;
        }
      }
    }

    if (now - lastDraw >= 300) {
      lastDraw = now;
      const int16_t rowH = 30, x0 = 8, barX = SW() / 2, barW = SW() / 2 - 12;
      for (uint8_t c = 0; c < 4; c++) {
        uint16_t v = lightRead(c);
        int16_t y = STATUS_H + 8 + c * rowH;
        lcd.fillRect(0, y, SW(), rowH - 4, C_BG);
        char l[40];
        snprintf(l, sizeof(l), "%s: %4u", lightChName(c), v);
        useFont(font_hu16);
        lcd.setTextColor(c == g_set.lightCh ? C_OK : C_TEXT, C_BG);
        lcd.setTextDatum(lgfx::textdatum::middle_left);
        lcd.drawString(l, x0, y + (rowH - 4) / 2);
        lcd.drawRect(barX, y + 4, barW, rowH - 12, C_KEY_EDGE);
        lcd.fillRect(barX + 1, y + 5, (int32_t)(barW - 2) * v / 4095, rowH - 14, c == g_set.lightCh ? C_OK : C_DIM);
      }
      // aktuális szint (ha be van tanítva)
      int16_t yl = STATUS_H + 8 + 4 * rowH + 6;
      lcd.fillRect(0, yl - 10, SW(), 20, C_BG);
      if (g_set.lightCh != LIGHT_NONE) {
        uint16_t lvl = lightLevel(lightRead(g_set.lightCh), g_set.lightDark, g_set.lightBright);
        char l[48];
        snprintf(l, sizeof(l), tr("Level: %u%%  (%s)", "Szint: %u%%  (%s)"), (unsigned)(lvl / 10), lightChName(g_set.lightCh));
        drawCentered(l, SW() / 2, yl, font_hu16, C_OK, C_BG);
      } else {
        drawCentered(tr("Not calibrated", "Nincs betanítva"), SW() / 2, yl, font_hu16, C_WARN, C_BG);
      }
      if (resDirty) {
        resDirty = false;
        lcd.fillRect(0, yl + 14, SW(), 22, C_BG);
        if (res[0]) drawFit(res, SW() / 2, yl + 25, SW() - 8, C_WARN, C_BG);
      }
    }
    delay(2);
  }
  while (lcd.getTouch(&tx, &ty) || digitalRead(PIN_BOOT) == LOW) delay(10);
  delay(100);
}

static void showRestart() {
  settingsSave();
  lcd.fillScreen(C_BG);
  drawCentered(tr("Restarting...", "Újraindítás..."), SW() / 2, SH() / 2, font_hu22, C_TEXT, C_BG);
  delay(600);
  ESP.restart();
}

static void settingAction(uint8_t i) {
  SetId id = setIdAt(i);
  switch (id) {
    case S_CAL:
      doCalibrate();
      drawSettings();
      return;
    case S_ROT:
      g_set.rotation = nextRotation(g_set.rotation);
      settingsSave();
      lcd.setRotation(g_set.rotation);
      doCalibrate();          // új tájolásnál újrakalibrálunk
      drawSettings();
      return;
    case S_BRIGHT: {
      // 25% -> 50% -> 75% -> 100% -> Auto -> 25%
      static const uint8_t lv[4] = {64, 128, 192, 255};
      if (g_set.autoBright) {
        g_set.autoBright = false;
        g_set.brightness = lv[0];
      } else if (g_set.brightness >= lv[3]) {
        g_set.autoBright = true;
        s_lightReset = true;
      } else {
        uint8_t idx = 0;
        for (uint8_t j = 0; j < 4; j++) if (g_set.brightness >= lv[j]) idx = j;
        g_set.brightness = lv[idx + 1];
      }
      if (!g_set.autoBright || g_set.lightCh == LIGHT_NONE) lcd.setBrightness(g_set.brightness);
      break;
    }
    case S_BEEP:
      g_set.beep = !g_set.beep;
      break;
    case S_LANG:
      g_set.lang = isHu() ? LANG_EN : LANG_HU;
      settingsSave();
      drawSettings();         // minden felirat nyelve változik
      return;
    case S_INV:
      g_set.invert = !g_set.invert;
      lcd.invertDisplay(g_set.invert);
      break;
    case S_BGR:
      g_set.bgr = !g_set.bgr;
      showRestart();
      return;
    case S_PANEL:
#if defined(BOARD_E32R35T)
      return;                 // 3.5"-on csak ST7796 van
#else
      g_set.panelSt7789 = !g_set.panelSt7789;
      g_set.invert = g_set.panelSt7789;
      showRestart();
      return;
#endif
    case S_MORE:
      s_setPage = 1;
      drawSettings();
      return;
    case S_BACK:
      s_setPage = 0;
      drawSettings();
      return;
    case S_EXIT:
      settingsSave();
      enterScreen(Screen::Main);
      return;
    case S_MODE:
      g_set.mode = isMuller() ? MODE_AUXN : MODE_MULLER;
      applyMode();
      break;
    case S_PROFILE: {
      // Vontatott -> Vont. JB3 -> Önjáró -> Vontatott (egyéniről a Vontatottra)
      uint8_t p = (g_set.mProfile >= PROF_CUSTOM) ? PROF_TOWED_OWN : (uint8_t)((g_set.mProfile + 1) % PROF_COUNT_PRESET);
      mullerApplyProfile(p, g_set.mCfg);
      g_set.mProfile = p;
      fixMullerLayer();
      Serial.printf("[ME] gépprofil: %s\n", mullerProfileName(p, false));
      break;
    }
    case S_EDIT:
      s_editLayer = g_set.mullerLayer;
      settingsSave();
      enterScreen(Screen::Editor);
      return;
    case S_KEYS:
      g_set.layout = nextLayout(g_set.layout);
      break;
    case S_CANINFO:
      showCanInfo();
      drawSettings();
      return;
    case S_LIGHT:
      showLightSensor();
      drawSettings();
      return;
  }
  settingsSave();
  drawSettingItem(i, false);
}

// Szerkesztő érintés: fül -> kijelölés; gomb -> állapot léptetés; Kész -> vissza a beállításokhoz
// Kódok s_setPressed-ben: 0..7 = gomb, 10..12 = fül (index), 20 = Kész
static int8_t editorHit(int16_t x, int16_t y) {
  if (layoutHitTest(&s_editDone, 1, x, y, 0) == 0) return 20;
  int8_t t = layoutHitTest(s_tabs, s_tabCount, x, y, 2);
  if (t >= 0) return 10 + t;
  return layoutHitTest(s_keys, 8, x, y, BUTTON_DEADZONE);
}

static void editorAction(int8_t hit) {
  if (hit == 20) {
    g_set.mProfile = mullerProfileOf(g_set.mCfg);
    fixMullerLayer();
    settingsSave();
    s_setPage = 1;
    enterScreen(Screen::Settings);
    return;
  }
  if (hit >= 10 && hit < 13) {
    s_editLayer = s_tabLayer[hit - 10];
    drawTabs(true, s_editLayer);
    for (uint8_t i = 0; i < 8; i++) drawMullerKeyEx(i, s_editLayer, false, true);
    drawLayerDot(s_editLayer);
    return;
  }
  if (hit >= 0 && hit < 8) {
    uint8_t in = mullerInputOf(s_editLayer, hit + 1);
    if (!mullerAssigned(in)) { fbBeep(600, 80); return; }   // kiosztatlan, nem szerkeszthető
    g_set.mCfg[in] = mullerNextState(g_set.mCfg, in);
    g_set.mProfile = mullerProfileOf(g_set.mCfg);
    settingsSave();
    Serial.printf("[ME] szerk.: %s B%d -> %s\n", mullerLayerName(s_editLayer, false), hit + 1,
                  g_set.mCfg[in] == BTN_OFF ? "KI" : (g_set.mCfg[in] == BTN_ALT ? "ALT ikon" : "BE"));
    drawMullerKeyEx(hit, s_editLayer, false, true);
    // fejléc profilnév frissítése
    useFont(font_hu13);
    lcd.fillRect(SW() / 2, 0, s_editDone.x - SW() / 2 - 2, STATUS_H, C_BAR);
    lcd.setTextColor(C_DIM, C_BAR);
    lcd.setTextDatum(lgfx::textdatum::middle_right);
    lcd.drawString(mullerProfileName(g_set.mProfile, isEn()), s_editDone.x - 8, STATUS_H / 2);
  }
}

static void handleTouch(uint32_t now) {
  int32_t x = 0, y = 0;
  bool touched = lcd.getTouch(&x, &y);
  bool changed = s_touch.update(touched, (int16_t)x, (int16_t)y, now);
  if (!changed) return;

  if (s_screen == Screen::Main && isMuller()) {
    if (s_touch.pressed()) {
      int8_t t = layoutHitTest(s_tabs, s_tabCount, s_touch.x(), s_touch.y(), 2);
      if (t >= 0) {
        uint8_t l = s_tabLayer[t];
        if (l != g_set.mullerLayer) { fbBeep(1800, 30); setMullerLayer(l); }
        return;
      }
      int8_t b = (s_keyCount == 8) ? layoutHitTest(s_keys, 8, s_touch.x(), s_touch.y(), BUTTON_DEADZONE) : -1;
      if (b >= 0 && !mullerBtnActive(g_set.mCfg, mullerInputOf(g_set.mullerLayer, b + 1))) b = -1;  // rejtett gomb
      setActiveKey(b);
    } else {
      if (s_touch.blocked()) Serial.println("[TOUCH] ugrás tartás közben -> elengedve");
      setActiveKey(-1);
    }
  } else if (s_screen == Screen::Main) {
    if (s_touch.pressed()) {
      setActiveKey(layoutHitTest(s_keys, s_keyCount, s_touch.x(), s_touch.y(), BUTTON_DEADZONE));
    } else {
      if (s_touch.blocked()) Serial.println("[TOUCH] ugrás tartás közben -> elengedve");
      setActiveKey(-1);
    }
  } else if (s_screen == Screen::Editor) {
    // felengedésre hajtjuk végre
    if (s_touch.pressed()) {
      s_setPressed = editorHit(s_touch.x(), s_touch.y());
      if (s_setPressed >= 0 && s_setPressed < 8) drawMullerKeyEx(s_setPressed, s_editLayer, true, true);
      if (s_setPressed >= 0) fbBeep(2400, 25);
    } else {
      int8_t it = s_setPressed;
      s_setPressed = -1;
      if (it >= 0 && it < 8) drawMullerKeyEx(it, s_editLayer, false, true);
      if (it >= 0 && !s_touch.blocked()) editorAction(it);
    }
  } else {
    // beállításoknál felengedésre hajtjuk végre (ne ismétlődjön)
    if (s_touch.pressed()) {
      s_setPressed = layoutHitTest(s_setRects, s_setCount, s_touch.x(), s_touch.y(), 2);
      if (s_setPressed >= 0) { drawSettingItem(s_setPressed, true); fbBeep(2400, 25); }
    } else {
      int8_t it = s_setPressed;
      s_setPressed = -1;
      if (it >= 0) {
        drawSettingItem(it, false);
        if (!s_touch.blocked()) settingAction(it);
      }
    }
  }
}

static void handleBoot(uint32_t now) {
  bool down = digitalRead(PIN_BOOT) == LOW;
  if (down && !s_bootDown) {
    s_bootDown = true;
    s_bootT0 = now;
    s_bootLongFired = false;
  } else if (down && s_bootDown) {
    if (!s_bootLongFired && now - s_bootT0 >= 2000) {
      s_bootLongFired = true;
      fbBeep(1200, 120);
      if (s_screen == Screen::Main) { s_setPage = 0; enterScreen(Screen::Settings); }
      else                          enterScreen(Screen::Main);
    }
  } else if (!down && s_bootDown) {
    s_bootDown = false;
    uint32_t dur = now - s_bootT0;
    if (!s_bootLongFired && dur >= 40 && dur < 800) {
      if (s_screen == Screen::Main) {
        if (s_activeKey < 0 && isMuller()) {
          uint8_t nl = mullerNextVisible(g_set.mCfg, g_set.mullerLayer);
          if (nl != 0xFF && nl != g_set.mullerLayer) { fbBeep(1800, 40); setMullerLayer(nl); }
        } else if (s_activeKey < 0) {
          g_set.layout = nextLayout(g_set.layout);
          settingsSave();
          Serial.printf("[UI] elrendezés: %u gomb\n", g_set.layout);
          fbBeep(1800, 40);
          drawMain();
        }
      } else if (s_screen == Screen::Editor) {
        editorAction(20);    // BOOT = Kész
      } else {
        enterScreen(Screen::Main);
      }
    }
  }
}

static void updateLed() {
  switch (canStats().state) {
    case CanState::Ok:           fbSetMode(LedMode::Green); break;
    case CanState::NoTraffic:    fbSetMode(LedMode::YellowBlink); break;
    case CanState::DriverFail:   fbSetMode(LedMode::RedBlink); break;
    default:                     fbSetMode(LedMode::Red); break;
  }
}

// =============================================================
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("=== " FW_NAME " v" FW_VERSION " ===");
  Serial.println(FW_CREDIT);
  Serial.println("Lap: " BOARD_NAME);

  pinMode(PIN_BOOT, INPUT_PULLUP);
  settingsLoad();
  Serial.printf("[SET] panel=%s invert=%d bgr=%d layout=%u cal=%d mód=%s nyelv=%s profil=%s\n",
                g_set.panelSt7789 ? "ST7789" : "ILI9341", g_set.invert, g_set.bgr, g_set.layout, g_set.calValid,
                g_set.mode == MODE_MULLER ? "Müller" : "AUX-N", isHu() ? "HU" : "EN",
                mullerProfileName(g_set.mProfile, false));

  fbBegin();

  lcd.selectPanel(g_set.panelSt7789, g_set.invert, g_set.bgr);
  lcd.init();
  lcd.setRotation(g_set.rotation);
  lcd.setBrightness(g_set.brightness);   // Auto módban a loop azonnal felülírja
  if (g_set.calValid) lcd.setTouchCalibrate(g_set.cal);

  selfTest();
  splash();

  canBegin();
  canSetRxHook([](uint32_t id, const uint8_t* d, uint8_t len) { s_muller.handleFrame(id, d, len, millis()); });
  s_muller.send = [](uint32_t id, const uint8_t* d, uint8_t len) { return canSend(id, d, len); };
  applyMode();
  enterScreen(Screen::Main);
}

void loop() {
  uint32_t now = millis();

  canLoop();
  fbLoop();

  // Müller küldő: tartás-ismétlés (200 ms) és életjel. Itt, a loop elején, nem blokkolva.
  s_muller.update(now);
  if (isMuller() && s_activeKey >= 0 && canStats().state == CanState::BusOff) {
    Serial.println("[ME] CAN bus-off -> elengedés");
    setActiveKey(-1);
  }

  if (now - s_lastTouchMs >= TOUCH_SAMPLE_MS) {
    s_lastTouchMs = now;
    handleTouch(now);
  }

  handleBoot(now);
  autoBrightLoop(now);

  if (now - s_lastBarMs >= 300) {
    s_lastBarMs = now;
    updateLed();
    if (s_screen == Screen::Main) drawStatusBar(false);
  }

  static uint32_t lastLog = 0;
  if (now - lastLog >= 5000) {
    lastLog = now;
    const CanStats& cs = canStats();
    Serial.printf("[CAN] %s  %lu f/s  %u eszköz  VT:%s  TEC=%lu REC=%lu  össz=%lu\n",
                  canStateText(cs.state, true), (unsigned long)cs.fps, cs.nodes,
                  cs.vtSeen ? "igen" : "nem", (unsigned long)cs.txErr,
                  (unsigned long)cs.rxErr, (unsigned long)cs.totalRx);
  }

  delay(1);
}
