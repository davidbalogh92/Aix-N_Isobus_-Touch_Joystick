// =============================================================
//  AUX-N Touch Joystick  -  1. fázis: hardver ébresztés
//  - kijelző + önteszt (színsávok, LED, hang)
//  - érintés kalibrálás (NVS-ben tárolva)
//  - 3 / 6 / 8 gombos elrendezés (BOOT rövid nyomás váltja)
//  - gombnyomás visszajelzés: kijelző + kék LED + hang + soros log
//  - CAN figyelés (250k, csak hallgat): forgalom, eszközszám, VT jelenlét
//  - beállítások menü (BOOT 2 mp)
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

static LGFX lcd;
static inline int16_t SW() { return (int16_t)lcd.width(); }
static inline int16_t SH() { return (int16_t)lcd.height(); }

#if defined(BOARD_E32R35T)
static const bool PANEL_SELECTABLE = false;
#else
static const bool PANEL_SELECTABLE = true;
#endif

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
enum class Screen : uint8_t { Main, Settings };
static Screen s_screen = Screen::Main;

static KeyRect s_keys[MAX_KEYS];
static uint8_t s_keyCount = 0;
static int8_t  s_activeKey = -1;
bool g_keyDown[MAX_KEYS] = {false};   // később ezt olvassa az AUX-N réteg

static TouchFilter s_touch({TOUCH_PRESS_STABLE_MS, TOUCH_RELEASE_MS,
                            TOUCH_STABLE_RADIUS, TOUCH_JUMP_RADIUS});

static char     s_barCache[96] = "";
static uint32_t s_lastBarMs = 0;
static uint32_t s_lastTouchMs = 0;

// beállítások menü
static const uint8_t SET_ITEMS = 10;
static const uint8_t SET_BACK = 9;
static KeyRect s_setRects[SET_ITEMS];
static int8_t  s_setPressed = -1;

// BOOT gomb
static bool     s_bootDown = false;
static uint32_t s_bootT0 = 0;
static bool     s_bootLongFired = false;

// =============================================================
//  Rajzolás
// =============================================================
static void drawCentered(const char* txt, int16_t x, int16_t y, const uint8_t* font,
                         uint16_t fg, uint16_t bg) {
  useFont(font);
  lcd.setTextColor(fg, bg);
  lcd.setTextDatum(lgfx::textdatum::middle_center);
  lcd.drawString(txt, x, y);
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
    case 0: return "Álló";
    case 1: return "Fekvő";
    case 2: return "Álló 180°";
    default: return "Fekvő 180°";
  }
}

static void drawStatusBar(bool force) {
  const CanStats& cs = canStats();
  char left[16], mid[48], right[24];
  snprintf(left, sizeof(left), "%u gomb", g_set.layout);
  if (cs.state == CanState::Ok && cs.vtSeen) {
    snprintf(mid, sizeof(mid), "CAN: OK, VT:%u", cs.vtAddr);
  } else {
    snprintf(mid, sizeof(mid), "%s", canStateText(cs.state));
  }
  if (cs.state == CanState::Ok && SW() >= 300) {
    snprintf(right, sizeof(right), "%u eszköz", cs.nodes);
  } else {
    snprintf(right, sizeof(right), "%s", "");
  }

  char all[96];
  snprintf(all, sizeof(all), "%s|%s|%s", left, mid, right);
  if (!force && strcmp(all, s_barCache) == 0) return;
  strncpy(s_barCache, all, sizeof(s_barCache) - 1);

  uint16_t stc = C_WARN;
  switch (cs.state) {
    case CanState::Ok:           stc = C_OK; break;
    case CanState::NoTraffic:    stc = C_WARN; break;
    default:                     stc = C_ERR; break;
  }

  lcd.fillRect(0, 0, SW(), STATUS_H, C_BAR);
  useFont(font_hu16);
  lcd.setTextColor(C_TEXT, C_BAR);
  lcd.setTextDatum(lgfx::textdatum::middle_left);
  lcd.drawString(left, 6, STATUS_H / 2);
  lcd.setTextColor(stc, C_BAR);
  lcd.drawString(mid, 80, STATUS_H / 2);
  lcd.setTextColor(C_DIM, C_BAR);
  lcd.setTextDatum(lgfx::textdatum::middle_right);
  lcd.drawString(right, SW() - 4, STATUS_H / 2);
}

static void drawKey(uint8_t i, bool on) {
  const KeyRect& k = s_keys[i];
  uint16_t fill = on ? C_KEY_ON : C_KEY;
  lcd.fillRoundRect(k.x, k.y, k.w, k.h, 10, fill);
  lcd.drawRoundRect(k.x, k.y, k.w, k.h, 10, on ? C_TEXT : C_KEY_EDGE);
  char num[4];
  snprintf(num, sizeof(num), "%u", i + 1);
  drawCentered(num, k.x + k.w / 2, k.y + k.h / 2, font_hu40, on ? RGB565(0, 0, 0) : C_TEXT, fill);
}

static void drawMain() {
  lcd.fillScreen(C_BG);
  s_keyCount = layoutCompute(g_set.layout, 0, STATUS_H, SW(), SH() - STATUS_H,
                             s_keys, MAX_KEYS);
  for (uint8_t i = 0; i < s_keyCount; i++) drawKey(i, (int8_t)i == s_activeKey);
  drawStatusBar(true);
}

// ---- beállítások ----
static void settingLabel(uint8_t i, char* buf, size_t n) {
  switch (i) {
    case 0: snprintf(buf, n, "Kalibrálás"); break;
    case 1: snprintf(buf, n, "Gombok: %u", g_set.layout); break;
#if defined(BOARD_E32R35T)
    case 2: snprintf(buf, n, "Panel: ST7796"); break;
#else
    case 2: snprintf(buf, n, "Panel: %s", g_set.panelSt7789 ? "ST7789" : "ILI9341"); break;
#endif
    case 3: snprintf(buf, n, "Invert: %s", g_set.invert ? "Be" : "Ki"); break;
    case 4: snprintf(buf, n, "Szín: %s", g_set.bgr ? "BGR" : "RGB"); break;
    case 5: snprintf(buf, n, "Fény: %u%%", (unsigned)((g_set.brightness * 100 + 127) / 255)); break;
    case 6: snprintf(buf, n, "Hang: %s", g_set.beep ? "Be" : "Ki"); break;
    case 7: snprintf(buf, n, "Tájolás: %s", rotationName(g_set.rotation)); break;
    case 8: snprintf(buf, n, "Info: CAN"); break;
    case 9: snprintf(buf, n, "Vissza"); break;
  }
}

static void drawSettingItem(uint8_t i, bool on) {
  const KeyRect& k = s_setRects[i];
  uint16_t fill = on ? C_KEY_ON : (i == SET_BACK ? RGB565(40, 90, 60) : C_KEY);
  lcd.fillRoundRect(k.x, k.y, k.w, k.h, 8, fill);
  lcd.drawRoundRect(k.x, k.y, k.w, k.h, 8, C_KEY_EDGE);
  char buf[32];
  settingLabel(i, buf, sizeof(buf));
  drawCentered(buf, k.x + k.w / 2, k.y + k.h / 2, font_hu16, on ? RGB565(0, 0, 0) : C_TEXT, fill);
}

static void drawSettings() {
  lcd.fillScreen(C_BG);
  lcd.fillRect(0, 0, SW(), STATUS_H, C_BAR);
  useFont(font_hu16);
  lcd.setTextColor(C_TEXT, C_BAR);
  lcd.setTextDatum(lgfx::textdatum::middle_left);
  lcd.drawString("Beállítások", 6, STATUS_H / 2);
  lcd.setTextColor(C_DIM, C_BAR);
  lcd.setTextDatum(lgfx::textdatum::middle_right);
  lcd.drawString("v" FW_VERSION, SW() - 6, STATUS_H / 2);

  const int16_t top = STATUS_H + 6, footer = 22, gap = 6;
  const int16_t w = (SW() - 3 * gap) / 2;
  const int16_t rows = (SET_ITEMS + 1) / 2;
  const int16_t h = (SH() - top - footer - rows * gap) / rows;
  for (uint8_t i = 0; i < SET_ITEMS; i++) {
    s_setRects[i].x = gap + (i % 2) * (w + gap);
    s_setRects[i].y = top + (i / 2) * (h + gap);
    s_setRects[i].w = w;
    s_setRects[i].h = h;
    drawSettingItem(i, false);
  }
  drawCredit(SH() - footer / 2 - 1, C_DIM);
}

// =============================================================
//  Önteszt, nyitókép, kalibrálás
// =============================================================
static void selfTest() {
  const char* names[4] = {"PIROS", "ZÖLD", "KÉK", "FEHÉR"};
  const uint16_t cols[4] = {RGB565(255, 0, 0), RGB565(0, 255, 0), RGB565(0, 0, 255), RGB565(255, 255, 255)};
  const int16_t w = SW() / 4;
  for (int i = 0; i < 4; i++) {
    lcd.fillRect(i * w, 0, w, SH(), cols[i]);
    drawCentered(names[i], i * w + w / 2, SH() / 2, font_hu16,
                 i == 3 ? RGB565(0, 0, 0) : RGB565(255, 255, 255), cols[i]);
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
  drawCentered("Érintés kalibrálás", SW() / 2, SH() / 2 - 30, font_hu22, C_TEXT, C_BG);
  drawCentered("Érintsd meg sorban", SW() / 2, SH() / 2 + 10, font_hu16, C_DIM, C_BG);
  drawCentered("a nyilak hegyét", SW() / 2, SH() / 2 + 32, font_hu16, C_DIM, C_BG);
  delay(1500);
  forgetFont();
  const uint16_t fg = RGB565(255, 255, 255), bg = C_BG;
  lcd.calibrateTouch(g_set.cal, fg, bg, 20);
  lcd.setTouchCalibrate(g_set.cal);
  g_set.calValid = true;
  settingsSave();
  Serial.println("[TOUCH] kalibráció mentve");
  fbBeep(2000, 60);
  // várjuk meg, hogy felengedje
  int32_t x, y;
  while (lcd.getTouch(&x, &y)) delay(10);
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
  drawCentered("Érintés 1 mp: kalibrálás", SW() / 2, cy + 30, font_hu16, C_DIM, C_BG);
#if !defined(BOARD_E32R35T)
  drawCentered("BOOT 2 mp: panel váltás", SW() / 2, cy + 52, font_hu16, C_DIM, C_BG);
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
static void setActiveKey(int8_t k) {
  if (k == s_activeKey) return;
  int8_t old = s_activeKey;
  s_activeKey = k;
  if (old >= 0) {
    g_keyDown[old] = false;
    Serial.printf("[KEY] %d FEL\n", old + 1);
    if (s_screen == Screen::Main && old < s_keyCount) drawKey(old, false);
  }
  if (k >= 0) {
    g_keyDown[k] = true;
    Serial.printf("[KEY] %d LE\n", k + 1);
    if (s_screen == Screen::Main) drawKey(k, true);
    fbBeep(2400, 25);
  }
  fbKeyActive(k >= 0);
}

static void enterScreen(Screen s) {
  setActiveKey(-1);
  s_setPressed = -1;
  s_screen = s;
  if (s == Screen::Main) drawMain(); else drawSettings();
}

// CAN diagnosztika képernyő: érintésre vagy BOOT-ra kilép
static void showCanInfo() {
  int32_t x, y;
  while (lcd.getTouch(&x, &y)) delay(10);   // előbb engedje fel
  lcd.fillScreen(C_BG);
  lcd.fillRect(0, 0, SW(), STATUS_H, C_BAR);
  useFont(font_hu16);
  lcd.setTextColor(C_TEXT, C_BAR);
  lcd.setTextDatum(lgfx::textdatum::middle_left);
  lcd.drawString("CAN diagnosztika", 6, STATUS_H / 2);
  drawCentered("Érintés: vissza", SW() / 2, SH() - 14, font_hu16, C_DIM, C_BG);

  uint32_t last = 0;
  while (true) {
    canLoop();
    fbLoop();
    if (lcd.getTouch(&x, &y) || digitalRead(PIN_BOOT) == LOW) break;
    uint32_t now = millis();
    if (now - last < 300) { delay(5); continue; }
    last = now;
    const CanStats& cs = canStats();
    char l[8][40];
    snprintf(l[0], 40, "%s", canStateText(cs.state));
    snprintf(l[1], 40, "Forgalom: %lu frame/s", (unsigned long)cs.fps);
    snprintf(l[2], 40, "Eszközök: %u", cs.nodes);
    if (cs.vtSeen) snprintf(l[3], 40, "VT (terminál): cím %u", cs.vtAddr);
    else           snprintf(l[3], 40, "VT (terminál): nem látszik");
    snprintf(l[4], 40, "Hibaszámláló TX/RX: %lu / %lu", (unsigned long)cs.txErr, (unsigned long)cs.rxErr);
    snprintf(l[5], 40, "Összes frame: %lu", (unsigned long)cs.totalRx);
    snprintf(l[6], 40, "Lábak: TX=IO%d  RX=IO%d", PIN_CAN_TX, PIN_CAN_RX);
    snprintf(l[7], 40, "Sebesség: 250 kbit/s");
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

static void settingAction(uint8_t i) {
  switch (i) {
    case 0:
      doCalibrate();
      drawSettings();
      return;
    case 1:
      g_set.layout = nextLayout(g_set.layout);
      break;
    case 2:
    case 4: {
#if defined(BOARD_E32R35T)
      if (i == 2) return;   // 3.5"-on csak ST7796 van
#endif
      if (i == 2) { g_set.panelSt7789 = !g_set.panelSt7789; g_set.invert = g_set.panelSt7789; }
      else        { g_set.bgr = !g_set.bgr; }
      settingsSave();
      lcd.fillScreen(C_BG);
      drawCentered("Újraindítás...", SW() / 2, SH() / 2, font_hu22, C_TEXT, C_BG);
      delay(600);
      ESP.restart();
      return;
    }
    case 3:
      g_set.invert = !g_set.invert;
      lcd.invertDisplay(g_set.invert);
      break;
    case 5: {
      static const uint8_t lv[4] = {64, 128, 192, 255};
      uint8_t idx = 0;
      for (uint8_t j = 0; j < 4; j++) if (g_set.brightness >= lv[j]) idx = j;
      g_set.brightness = lv[(idx + 1) % 4];
      lcd.setBrightness(g_set.brightness);
      break;
    }
    case 6:
      g_set.beep = !g_set.beep;
      break;
    case 7:
      g_set.rotation = nextRotation(g_set.rotation);
      settingsSave();
      lcd.setRotation(g_set.rotation);
      setActiveKey(-1);
      doCalibrate();          // új tájolásnál újrakalibrálunk
      drawSettings();
      return;
    case 8:
      showCanInfo();
      drawSettings();
      return;
    case SET_BACK:
      settingsSave();
      enterScreen(Screen::Main);
      return;
  }
  settingsSave();
  drawSettingItem(i, false);
}

static void handleTouch(uint32_t now) {
  int32_t x = 0, y = 0;
  bool touched = lcd.getTouch(&x, &y);
  bool changed = s_touch.update(touched, (int16_t)x, (int16_t)y, now);
  if (!changed) return;

  if (s_screen == Screen::Main) {
    if (s_touch.pressed()) {
      setActiveKey(layoutHitTest(s_keys, s_keyCount, s_touch.x(), s_touch.y(), BUTTON_DEADZONE));
    } else {
      if (s_touch.blocked()) Serial.println("[TOUCH] ugrás tartás közben -> elengedve");
      setActiveKey(-1);
    }
  } else {
    // beállításoknál felengedésre hajtjuk végre (ne ismétlődjön)
    if (s_touch.pressed()) {
      s_setPressed = layoutHitTest(s_setRects, SET_ITEMS, s_touch.x(), s_touch.y(), 2);
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
      enterScreen(s_screen == Screen::Main ? Screen::Settings : Screen::Main);
    }
  } else if (!down && s_bootDown) {
    s_bootDown = false;
    uint32_t dur = now - s_bootT0;
    if (!s_bootLongFired && dur >= 40 && dur < 800) {
      if (s_screen == Screen::Main) {
        if (s_activeKey < 0) {
          g_set.layout = nextLayout(g_set.layout);
          settingsSave();
          Serial.printf("[UI] elrendezés: %u gomb\n", g_set.layout);
          fbBeep(1800, 40);
          drawMain();
        }
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
  Serial.printf("[SET] panel=%s invert=%d bgr=%d layout=%u cal=%d\n",
                g_set.panelSt7789 ? "ST7789" : "ILI9341", g_set.invert, g_set.bgr,
                g_set.layout, g_set.calValid);

  fbBegin();

  lcd.selectPanel(g_set.panelSt7789, g_set.invert, g_set.bgr);
  lcd.init();
  lcd.setRotation(g_set.rotation);
  lcd.setBrightness(g_set.brightness);
  if (g_set.calValid) lcd.setTouchCalibrate(g_set.cal);

  selfTest();
  splash();

  canBegin();
  enterScreen(Screen::Main);
}

void loop() {
  uint32_t now = millis();

  canLoop();
  fbLoop();

  if (now - s_lastTouchMs >= TOUCH_SAMPLE_MS) {
    s_lastTouchMs = now;
    handleTouch(now);
  }

  handleBoot(now);

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
                  canStateText(cs.state), (unsigned long)cs.fps, cs.nodes,
                  cs.vtSeen ? "igen" : "nem", (unsigned long)cs.txErr,
                  (unsigned long)cs.rxErr, (unsigned long)cs.totalRx);
  }

  delay(1);
}
