#include "feedback.h"
#include "config.h"
#include "settings.h"

static const uint8_t BEEP_CH = 2;   // LEDC csatorna (a háttérfény a 7-est használja)

static LedMode  s_mode       = LedMode::Off;
static bool     s_keyActive  = false;
static uint32_t s_beepEnd    = 0;
static bool     s_beeping    = false;

void fbLedRaw(bool r, bool g, bool b) {
  // közös anód: LOW = világít
  digitalWrite(PIN_LED_R, r ? LOW : HIGH);
  digitalWrite(PIN_LED_G, g ? LOW : HIGH);
  digitalWrite(PIN_LED_B, b ? LOW : HIGH);
}

void fbBegin() {
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  fbLedRaw(false, false, false);

  pinMode(PIN_SPK_EN, OUTPUT);
  digitalWrite(PIN_SPK_EN, HIGH);   // erősítő ki, amíg nem kell (zajmentes)
  ledcSetup(BEEP_CH, 2000, 8);
  ledcAttachPin(PIN_SPK, BEEP_CH);
  ledcWrite(BEEP_CH, 0);
}

void fbSetMode(LedMode m) { s_mode = m; }
void fbKeyActive(bool active) { s_keyActive = active; }

void fbBeep(uint16_t freq, uint16_t ms) {
  if (!g_set.beep) return;
  digitalWrite(PIN_SPK_EN, LOW);
  ledcWriteTone(BEEP_CH, freq);
  s_beepEnd = millis() + ms;
  s_beeping = true;
}

void fbLoop() {
  uint32_t now = millis();

  if (s_beeping && (int32_t)(now - s_beepEnd) >= 0) {
    ledcWriteTone(BEEP_CH, 0);
    ledcWrite(BEEP_CH, 0);
    digitalWrite(PIN_SPK_EN, HIGH);
    s_beeping = false;
  }

  if (s_keyActive) {
    fbLedRaw(false, false, true);
    return;
  }

  bool blinkOn = ((now / 400) % 2) == 0;
  switch (s_mode) {
    case LedMode::Off:         fbLedRaw(false, false, false); break;
    case LedMode::Green:       fbLedRaw(false, true, false); break;
    case LedMode::YellowBlink: fbLedRaw(blinkOn, blinkOn, false); break;
    case LedMode::Red:         fbLedRaw(true, false, false); break;
    case LedMode::RedBlink:    fbLedRaw(blinkOn, false, false); break;
  }
}
