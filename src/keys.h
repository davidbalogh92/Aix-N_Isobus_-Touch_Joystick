#pragma once
#include <stdint.h>

// Gombelrendezés + érintés szűrés. Hardverfüggetlen logika (később natív unit tesztelhető).

struct KeyRect {
  int16_t x, y, w, h;
};

// Elrendezés számítása a gombterületre (a státuszsor alatt).
// layout: 3 (3x1), 6 (3x2), 8 (4x2). Visszaadja a gombok számát.
uint8_t layoutCompute(uint8_t layout, int16_t areaX, int16_t areaY, int16_t areaW, int16_t areaH,
                      KeyRect* out, uint8_t maxOut);

// Müller joystick elrendezés: out[0..7] = B1..B8 téglalapja.
//   B1 | B3 B4
//   B2 | B5 B6
//      | B7 B8
// A bal oszlop és a jobb blokk között szélesebb rés van (mint a valódi joystickon).
void mullerLayout(int16_t areaX, int16_t areaY, int16_t areaW, int16_t areaH, KeyRect out[8]);

// Általános rács (cols x rows), soronként balról jobbra
void gridLayout(int16_t areaX, int16_t areaY, int16_t areaW, int16_t areaH, uint8_t cols, uint8_t rows,
                KeyRect* out);

// Melyik gombra esik a pont (a szélein lévő holtsávot levonva). -1 = egyik sem.
int8_t layoutHitTest(const KeyRect* rects, uint8_t count, int16_t x, int16_t y, int16_t deadzone);

// Rezisztív érintés szűrő:
//  - lenyomás csak stabil érintés után,
//  - elengedés rövid "nincs érintés" után,
//  - tartás közbeni nagy ugrás (pl. második ujj -> átlagolt koordináta) => azonnali elengedés,
//    és nem fogad új lenyomást, amíg teljesen fel nem engedték.
class TouchFilter {
 public:
  struct Params {
    uint16_t pressStableMs;
    uint16_t releaseMs;
    int16_t  stableRadius;
    int16_t  jumpRadius;
  };

  explicit TouchFilter(const Params& p) : _p(p) {}

  // Minden mintavételkor hívni. Visszaadja: true, ha az aktív "lenyomott pont" állapota változott.
  bool update(bool touched, int16_t x, int16_t y, uint32_t nowMs);

  bool    pressed() const { return _state == State::Pressed; }
  int16_t x() const { return _px; }
  int16_t y() const { return _py; }
  bool    blocked() const { return _state == State::Blocked; }

 private:
  enum class State : uint8_t { Idle, Candidate, Pressed, Blocked };
  Params   _p;
  State    _state = State::Idle;
  int16_t  _cx = 0, _cy = 0;   // jelölt kezdőpont
  int16_t  _px = 0, _py = 0;   // elfogadott pont
  uint32_t _t0 = 0;            // jelölt kezdete
  uint32_t _lastTouch = 0;     // utolsó érintett minta ideje
  bool     _hadTouch = false;
};
