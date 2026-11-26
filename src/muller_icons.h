#pragma once
// Müller funkció ikonok - saját rajzolás (vektoros primitívekből), a funkció jelentése szerint.
// Sablon a rajzoló "G" típusra, hogy PC-n is renderelhető legyen előnézetnek (tools/icon_preview.cpp).
//
// G-nek ezek kellenek:
//   void wideLine(float x0, float y0, float x1, float y1, float r, uint16_t c);
//   void fillTri(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c);
//   void fillRect(int x, int y, int w, int h, uint16_t c);
//   void ring(int x, int y, int r, float thick, uint16_t c);
//   void text(const char* s, int cx, int cy, float u, uint16_t fg, uint16_t bg);  // középre; u<7 -> kisebb font
#include <math.h>
#include <stdint.h>
#include "muller_funcs.h"

struct MIconColors {
  uint16_t fg;      // vonalak (sötét)
  uint16_t blue;    // permetsugár
  uint16_t red;     // mozgás nyilak
  uint16_t bg;      // gomb háttér (szövegnek)
};

namespace micon {

template <class G>
inline void arrow(G& g, float x0, float y0, float x1, float y1, float u, uint16_t c) {
  float dx = x1 - x0, dy = y1 - y0;
  float L = sqrtf(dx * dx + dy * dy);
  if (L < 1) return;
  float ux = dx / L, uy = dy / L;          // irány
  float px = -uy, py = ux;                 // merőleges
  float hl = 2.2f * u, hw = 1.5f * u;      // fej hossz / fél szélesség
  float bx = x1 - ux * hl, by = y1 - uy * hl;
  g.wideLine(x0, y0, bx, by, 0.45f * u, c);
  g.fillTri((int)x1, (int)y1, (int)(bx + px * hw), (int)(by + py * hw),
            (int)(bx - px * hw), (int)(by - py * hw), c);
}

// szórófej: test + szár + (be: kék permetkúp / ki: kis csúcs)
template <class G>
inline void nozzle(G& g, float x, float y, float u, bool on, const MIconColors& k) {
  g.fillRect((int)(x - 2.2f * u), (int)(y - 3.8f * u), (int)(4.4f * u), (int)(2.4f * u), k.fg);
  g.fillRect((int)(x - 1.5f * u), (int)(y - 3.2f * u), (int)(3.0f * u), (int)(1.0f * u), k.bg);
  g.fillRect((int)(x - 0.5f * u), (int)(y - 1.4f * u), (int)(1.0f * u), (int)(1.4f * u), k.fg);
  if (on) {
    g.fillTri((int)x, (int)(y - 0.2f * u), (int)(x - 2.4f * u), (int)(y + 3.8f * u),
              (int)(x + 2.4f * u), (int)(y + 3.8f * u), k.blue);
  } else {
    g.fillTri((int)x, (int)(y - 0.2f * u), (int)(x - 1.0f * u), (int)(y + 1.2f * u),
              (int)(x + 1.0f * u), (int)(y + 1.2f * u), k.fg);
  }
}

// szórókeret (tilt: -1 bal vége fent, +1 jobb vége fent, 0 vízszintes)
template <class G>
inline void boom(G& g, float cx, float cy, float u, int tilt, const MIconColors& k) {
  float t = 1.4f * u * (float)tilt;
  float xl = cx - 4.6f * u, xr = cx + 4.6f * u;
  float yl = cy - t, yr = cy + t;          // tilt=-1 -> bal fent
  if (tilt < 0) { yl = cy - 1.4f * u; yr = cy + 1.4f * u; }
  if (tilt > 0) { yl = cy + 1.4f * u; yr = cy - 1.4f * u; }
  g.wideLine(xl, yl, xr, yr, 0.45f * u, k.fg);
  // rácsozat: alsó párhuzamos vonal + összekötők
  float off = 1.1f * u;
  float ax = cx - 3.8f * u, bx = cx + 3.8f * u;
  auto yAt = [&](float x) { return yl + (yr - yl) * (x - xl) / (xr - xl); };
  g.wideLine(ax, yAt(ax) + off, bx, yAt(bx) + off, 0.25f * u, k.fg);
  for (int i = 0; i <= 4; i++) {
    float x = ax + (bx - ax) * i / 4.0f;
    float x2 = x + ((i % 2) ? -0.9f : 0.9f) * u;
    g.wideLine(x, yAt(x), x2, yAt(x2) + off, 0.2f * u, k.fg);
  }
  // középrész
  g.fillRect((int)(cx - 0.9f * u), (int)(yAt(cx) - 1.6f * u), (int)(1.8f * u), (int)(1.6f * u), k.fg);
}

// vonórúd (lean: -1 balra dől, 0 egyenes, +1 jobbra dől)
template <class G>
inline void drawbar(G& g, float cx, float cy, float u, int lean, const MIconColors& k) {
  float by = cy + 3.6f * u;
  g.wideLine(cx - 2.8f * u, by, cx + 2.8f * u, by, 0.4f * u, k.fg);              // tengely
  g.fillRect((int)(cx - 3.4f * u), (int)(by - 1.0f * u), (int)(1.0f * u), (int)(2.0f * u), k.fg);
  g.fillRect((int)(cx + 2.4f * u), (int)(by - 1.0f * u), (int)(1.0f * u), (int)(2.0f * u), k.fg);
  float tx = cx + 2.6f * u * (float)lean, ty = cy - 2.6f * u;
  g.wideLine(cx, by, tx, ty, 0.42f * u, k.fg);                                     // rúd
  g.ring((int)tx, (int)(ty - 0.9f * u), (int)(0.9f * u), 0.35f * u, k.fg);        // vonószem
}

}  // namespace micon

// Ikon rajzolása (cx, cy) középpel, s méretű négyzetbe.
template <class G>
inline void drawMullerIcon(G& g, MIcon ic, int cx, int cy, int s, const MIconColors& k) {
  using namespace micon;
  float u = s / 10.0f;
  switch (ic) {
    case MIcon::None:
      g.wideLine(cx - 2.5f * u, cy, cx + 2.5f * u, cy, 0.4f * u, k.fg);
      break;

    // ---- permetezés ----
    case MIcon::Target100:
      nozzle(g, cx - 3.2f * u, cy, u, true, k);
      g.text("100%", (int)(cx + 2.4f * u), cy, u, k.fg, k.bg);
      break;
    case MIcon::SectMain:
      nozzle(g, cx - 3.2f * u, cy, u, true, k);
      g.text("BE/KI", (int)(cx + 2.4f * u), cy, u, k.fg, k.bg);
      break;
    case MIcon::SectOnRL:       // <- szórófej(be): bekapcsolás jobbról balra
      arrow(g, cx - 0.4f * u, cy, cx - 4.8f * u, cy, u, k.fg);
      nozzle(g, cx + 2.6f * u, cy, u, true, k);
      break;
    case MIcon::SectOnLR:       // szórófej(be) ->: bekapcsolás balról jobbra
      nozzle(g, cx - 2.6f * u, cy, u, true, k);
      arrow(g, cx + 0.4f * u, cy, cx + 4.8f * u, cy, u, k.fg);
      break;
    case MIcon::SectOffLR:      // szórófej(ki) ->: kikapcsolás balról jobbra
      nozzle(g, cx - 2.6f * u, cy, u, false, k);
      arrow(g, cx + 0.4f * u, cy, cx + 4.8f * u, cy, u, k.fg);
      break;
    case MIcon::SectOffRL:      // <- szórófej(ki): kikapcsolás jobbról balra
      arrow(g, cx - 0.4f * u, cy, cx - 4.8f * u, cy, u, k.fg);
      nozzle(g, cx + 2.6f * u, cy, u, false, k);
      break;
    case MIcon::RateMinus:
      g.wideLine(cx - 4.6f * u, cy, cx - 1.6f * u, cy, 0.5f * u, k.fg);
      nozzle(g, cx + 2.0f * u, cy, u, true, k);
      break;
    case MIcon::RatePlus:
      g.wideLine(cx - 4.6f * u, cy, cx - 1.6f * u, cy, 0.5f * u, k.fg);
      g.wideLine(cx - 3.1f * u, cy - 1.5f * u, cx - 3.1f * u, cy + 1.5f * u, 0.5f * u, k.fg);
      nozzle(g, cx + 2.0f * u, cy, u, true, k);
      break;

    // ---- keret ----
    case MIcon::BoomAuto:
      boom(g, cx, cy - 1.8f * u, u, 0, k);
      g.text("AUTO", cx, (int)(cy + 3.0f * u), u, k.fg, k.bg);
      break;
    case MIcon::BoomUp:
      boom(g, cx, cy + 2.6f * u, u, 0, k);
      arrow(g, cx, cy + 0.2f * u, cx, cy - 4.8f * u, u, k.red);
      break;
    case MIcon::BoomDown:
      boom(g, cx, cy + 2.6f * u, u, 0, k);
      arrow(g, cx, cy - 4.8f * u, cx, cy + 0.2f * u, u, k.red);
      break;
    case MIcon::FoldIn:         // két vége befelé-felfelé
      boom(g, cx, cy + 2.6f * u, u, 0, k);
      arrow(g, cx - 4.6f * u, cy + 0.4f * u, cx - 1.6f * u, cy - 3.8f * u, u, k.red);
      arrow(g, cx + 4.6f * u, cy + 0.4f * u, cx + 1.6f * u, cy - 3.8f * u, u, k.red);
      break;
    case MIcon::FoldOut:        // két vége kifelé-lefelé
      boom(g, cx, cy + 2.6f * u, u, 0, k);
      arrow(g, cx - 1.6f * u, cy - 3.8f * u, cx - 4.6f * u, cy + 0.4f * u, u, k.red);
      arrow(g, cx + 1.6f * u, cy - 3.8f * u, cx + 4.6f * u, cy + 0.4f * u, u, k.red);
      break;
    case MIcon::TiltLeftUp:
      boom(g, cx, cy + 1.6f * u, u, -1, k);
      g.fillTri((int)(cx - 4.0f * u), (int)(cy - 3.6f * u), (int)(cx - 5.4f * u), (int)(cy - 1.4f * u),
                (int)(cx - 2.6f * u), (int)(cy - 1.4f * u), k.red);
      break;
    case MIcon::TiltRightUp:
      boom(g, cx, cy + 1.6f * u, u, 1, k);
      g.fillTri((int)(cx + 4.0f * u), (int)(cy - 3.6f * u), (int)(cx + 2.6f * u), (int)(cy - 1.4f * u),
                (int)(cx + 5.4f * u), (int)(cy - 1.4f * u), k.red);
      break;

    // ---- TRAIL ----
    case MIcon::TrailAuto:
      drawbar(g, cx - 2.4f * u, cy, u, 0, k);
      g.text("AUTO", (int)(cx + 3.0f * u), cy, u, k.fg, k.bg);
      break;
    case MIcon::TrailOnOff:
      drawbar(g, cx - 2.4f * u, cy, u, 0, k);
      g.text("BE/KI", (int)(cx + 3.0f * u), cy, u, k.fg, k.bg);
      break;
    case MIcon::TrailCenter:    // -> rúd <- : középre húzás
      drawbar(g, cx, cy, u, 0, k);
      arrow(g, cx - 5.0f * u, cy - 0.6f * u, cx - 1.4f * u, cy - 0.6f * u, u, k.red);
      arrow(g, cx + 5.0f * u, cy - 0.6f * u, cx + 1.4f * u, cy - 0.6f * u, u, k.red);
      break;
    case MIcon::SteerLeft:
      drawbar(g, cx - 1.8f * u, cy, u, -1, k);      // balra dőlő rúd, mellette balra mutató nyíl
      arrow(g, cx + 5.0f * u, cy, cx + 0.6f * u, cy, u, k.red);
      break;
    case MIcon::SteerRight:
      drawbar(g, cx + 1.8f * u, cy, u, 1, k);
      arrow(g, cx - 5.0f * u, cy, cx - 0.6f * u, cy, u, k.red);
      break;
  }
}
