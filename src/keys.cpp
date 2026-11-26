#include "keys.h"

static const int16_t GAP    = 8;   // gombok közötti rés (a keret bordái)
static const int16_t MARGIN = 4;   // szél

uint8_t layoutCompute(uint8_t layout, int16_t areaX, int16_t areaY, int16_t areaW, int16_t areaH,
                      KeyRect* out, uint8_t maxOut) {
  uint8_t cols, rows;
  switch (layout) {
    case 3:  cols = 3; rows = 1; break;
    case 8:  cols = 4; rows = 2; break;
    case 6:
    default: cols = 3; rows = 2; break;
  }
  int16_t cw = (areaW - 2 * MARGIN - (cols - 1) * GAP) / cols;
  int16_t ch = (areaH - 2 * MARGIN - (rows - 1) * GAP) / rows;
  uint8_t n = 0;
  for (uint8_t r = 0; r < rows; r++) {
    for (uint8_t c = 0; c < cols; c++) {
      if (n >= maxOut) return n;
      out[n].x = areaX + MARGIN + c * (cw + GAP);
      out[n].y = areaY + MARGIN + r * (ch + GAP);
      out[n].w = cw;
      out[n].h = ch;
      n++;
    }
  }
  return n;
}

int8_t layoutHitTest(const KeyRect* rects, uint8_t count, int16_t x, int16_t y, int16_t deadzone) {
  for (uint8_t i = 0; i < count; i++) {
    const KeyRect& k = rects[i];
    if (x >= k.x + deadzone && x < k.x + k.w - deadzone &&
        y >= k.y + deadzone && y < k.y + k.h - deadzone) {
      return (int8_t)i;
    }
  }
  return -1;
}

static inline bool within(int16_t ax, int16_t ay, int16_t bx, int16_t by, int16_t r) {
  int32_t dx = ax - bx, dy = ay - by;
  return dx * dx + dy * dy <= (int32_t)r * r;
}

bool TouchFilter::update(bool touched, int16_t x, int16_t y, uint32_t now) {
  if (touched) {
    _lastTouch = now;
    _hadTouch = true;
  }
  bool released = _hadTouch && !touched && (now - _lastTouch >= _p.releaseMs);

  switch (_state) {
    case State::Idle:
      if (touched) {
        _state = State::Candidate;
        _cx = x; _cy = y; _t0 = now;
      }
      return false;

    case State::Candidate:
      if (released) { _state = State::Idle; _hadTouch = false; return false; }
      if (!touched) return false;   // rövid kimaradás, várunk
      if (!within(x, y, _cx, _cy, _p.stableRadius)) {
        // mozog -> új jelölt
        _cx = x; _cy = y; _t0 = now;
        return false;
      }
      if (now - _t0 >= _p.pressStableMs) {
        _state = State::Pressed;
        _px = _cx; _py = _cy;
        return true;
      }
      return false;

    case State::Pressed:
      if (released) { _state = State::Idle; _hadTouch = false; return true; }
      if (touched && !within(x, y, _px, _py, _p.jumpRadius)) {
        // nagy ugrás tartás közben: 2. ujj vagy csúszás -> biztonsági elengedés
        _state = State::Blocked;
        return true;
      }
      return false;

    case State::Blocked:
      if (released) { _state = State::Idle; _hadTouch = false; }
      return false;
  }
  return false;
}
