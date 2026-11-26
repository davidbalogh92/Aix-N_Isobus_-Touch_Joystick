#pragma once
// Fényérzékelő (R21 fotoellenállás) logika - hardverfüggetlen, natívan tesztelhető.
// 4 mérési csatorna: 0 = IO34 11dB, 1 = IO34 0dB, 2 = IO39 11dB, 3 = IO39 0dB.
// Betanítás: "Sötét" és "Világos" minta -> a legnagyobb (nem telített) különbségű csatorna nyer.
#include <stdint.h>

static constexpr uint8_t  LIGHT_CHANNELS   = 4;
static constexpr uint16_t LIGHT_SATURATED  = 4050;   // 12 bites ADC teteje környéke
static constexpr uint16_t LIGHT_MIN_DIFF   = 120;    // ennél kisebb különbség = nincs érzékelő ezen a csatornán
static constexpr uint8_t  LIGHT_NONE       = 0xFF;

// A legjobb csatorna kiválasztása (LIGHT_NONE, ha egyik sem használható)
inline uint8_t lightPickChannel(const uint16_t dark[LIGHT_CHANNELS], const uint16_t bright[LIGHT_CHANNELS]) {
  uint8_t best = LIGHT_NONE;
  int bestDiff = 0;
  for (uint8_t c = 0; c < LIGHT_CHANNELS; c++) {
    if (dark[c] >= LIGHT_SATURATED && bright[c] >= LIGHT_SATURATED) continue;   // mindkettő telített
    int d = (int)bright[c] - (int)dark[c];
    if (d < 0) d = -d;
    if (d >= LIGHT_MIN_DIFF && d > bestDiff) { bestDiff = d; best = c; }
  }
  return best;
}

// Nyers érték -> 0..1000 (0 = sötét, 1000 = világos), az iránytól függetlenül
inline uint16_t lightLevel(uint16_t raw, uint16_t dark, uint16_t bright) {
  if (dark == bright) return 500;
  int num = (int)raw - (int)dark;
  int den = (int)bright - (int)dark;
  int v = num * 1000 / den;
  if (v < 0) v = 0;
  if (v > 1000) v = 1000;
  return (uint16_t)v;
}

// Szint (0..1000) -> fényerő (minB..maxB)
inline uint8_t lightBrightness(uint16_t level, uint8_t minB, uint8_t maxB) {
  if (level > 1000) level = 1000;
  return (uint8_t)(minB + ((int)(maxB - minB) * level + 500) / 1000);
}

// Simítás (EMA, alfa = 1/8), fixpontos: state = érték * 16
inline uint16_t lightSmooth(uint32_t& state16, uint16_t target, bool reset) {
  if (reset) state16 = (uint32_t)target * 16;
  else state16 = state16 - state16 / 8 + (uint32_t)target * 2;   // += (target*16 - state)/8
  return (uint16_t)((state16 + 8) / 16);
}
