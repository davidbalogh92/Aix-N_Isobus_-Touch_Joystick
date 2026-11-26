#pragma once
// Müller funkció ikonok - saját rajzolás, a JB3 joystick képi nyelvét követve:
// sötét gomb, fehér rajz, zöld nyilak, kék szórófej. Vektoros primitívekből.
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
  uint16_t fg;      // rajz (fehér)
  uint16_t blue;    // szórófej / permetsugár
  uint16_t arrow;   // mozgás nyilak (zöld)
  uint16_t bg;      // gomb háttér
  uint16_t accent;  // "AUTO" címke háttér (zöld)
};

namespace micon {

static constexpr float DEG = 3.14159265f / 180.0f;

template <class G>
inline void head(G& g, float x, float y, float ux, float uy, float u, uint16_t c) {
  // nyílhegy (x,y) csúccsal, (ux,uy) irányba mutat
  float px = -uy, py = ux, hl = 2.0f * u, hw = 1.5f * u;
  float bx = x - ux * hl, by = y - uy * hl;
  g.fillTri((int)x, (int)y, (int)(bx + px * hw), (int)(by + py * hw), (int)(bx - px * hw), (int)(by - py * hw), c);
}

template <class G>
inline void arrow(G& g, float x0, float y0, float x1, float y1, float u, uint16_t c) {
  float dx = x1 - x0, dy = y1 - y0, L = sqrtf(dx * dx + dy * dy);
  if (L < 1) return;
  float ux = dx / L, uy = dy / L;
  g.wideLine(x0, y0, x1 - ux * 1.8f * u, y1 - uy * 1.8f * u, 0.55f * u, c);
  head(g, x1, y1, ux, uy, u, c);
}

// ívelt nyíl: (cx,cy) középpontú, r sugarú ív a0 -> a1 fokig (képernyő: 0 = jobbra, 90 = le), hegy az a1 végén
template <class G>
inline void curvedArrow(G& g, float cx, float cy, float r, float a0, float a1, float u, uint16_t c) {
  const int N = 10;
  float dir = (a1 > a0) ? 1.0f : -1.0f;
  float aEnd = a1 - dir * (1.6f * u / r) / DEG;      // a hegy helyét kihagyjuk
  float px = cx + r * cosf(a0 * DEG), py = cy + r * sinf(a0 * DEG);
  for (int i = 1; i <= N; i++) {
    float a = a0 + (aEnd - a0) * i / N;
    float x = cx + r * cosf(a * DEG), y = cy + r * sinf(a * DEG);
    g.wideLine(px, py, x, y, 0.55f * u, c);
    px = x; py = y;
  }
  float ex = cx + r * cosf(a1 * DEG), ey = cy + r * sinf(a1 * DEG);
  float tx = -sinf(a1 * DEG) * dir, ty = cosf(a1 * DEG) * dir;   // érintő irány
  head(g, ex, ey, tx, ty, u, c);
}

// rácsos szórókeret (JB3 stílus), forgatható (fok, + = óramutató szerint -> jobb vége lejjebb)
template <class G>
inline void truss(G& g, float cx, float cy, float u, float rotDeg, uint16_t c, float len = 9.6f) {
  float ca = cosf(rotDeg * DEG), sa = sinf(rotDeg * DEG);
  auto P = [&](float x, float y, float& ox, float& oy) { ox = cx + x * ca - y * sa; oy = cy + x * sa + y * ca; };
  float hl = len * u / 2, h = 1.7f * u;
  float x0, y0, x1, y1;
  // felső öv (rövidebb) és alsó öv
  P(-hl + 1.0f * u, -h, x0, y0); P(hl - 1.0f * u, -h, x1, y1); g.wideLine(x0, y0, x1, y1, 0.26f * u, c);
  P(-hl, 0, x0, y0);             P(hl, 0, x1, y1);             g.wideLine(x0, y0, x1, y1, 0.3f * u, c);
  // végek
  P(-hl, 0, x0, y0); P(-hl + 1.0f * u, -h, x1, y1); g.wideLine(x0, y0, x1, y1, 0.24f * u, c);
  P(hl, 0, x0, y0);  P(hl - 1.0f * u, -h, x1, y1);  g.wideLine(x0, y0, x1, y1, 0.24f * u, c);
  // cikk-cakk rácsozat
  int n = 4;
  float a = -hl + 1.0f * u, b = hl - 1.0f * u;
  for (int i = 0; i < n; i++) {
    float xa = a + (b - a) * i / n, xb = a + (b - a) * (i + 0.5f) / n, xc = a + (b - a) * (i + 1) / n;
    P(xa, -h, x0, y0); P(xb, 0, x1, y1); g.wideLine(x0, y0, x1, y1, 0.17f * u, c);
    P(xb, 0, x0, y0);  P(xc, -h, x1, y1); g.wideLine(x0, y0, x1, y1, 0.17f * u, c);
  }
}

// szórófej: kis tartály + szár + kék kúp (be) / csak csúcs (ki)
template <class G>
inline void nozzle(G& g, float x, float y, float u, bool on, const MIconColors& k) {
  g.fillRect((int)(x - 2.0f * u), (int)(y - 3.8f * u), (int)(4.0f * u), (int)(2.2f * u), k.fg);
  g.fillRect((int)(x - 1.4f * u), (int)(y - 3.3f * u), (int)(2.8f * u), (int)(0.9f * u), k.bg);
  g.fillRect((int)(x - 0.45f * u), (int)(y - 1.6f * u), (int)(0.9f * u), (int)(1.5f * u), k.fg);
  if (on) {
    g.fillTri((int)x, (int)(y - 0.3f * u), (int)(x - 2.3f * u), (int)(y + 3.6f * u), (int)(x + 2.3f * u),
              (int)(y + 3.6f * u), k.blue);
  } else {
    g.fillTri((int)x, (int)(y - 0.3f * u), (int)(x - 1.0f * u), (int)(y + 1.1f * u), (int)(x + 1.0f * u),
              (int)(y + 1.1f * u), k.blue);
  }
}

// vonórúd A-keret (JB3): két szár egy pontba fut, tetején vonószem, alul tengely
template <class G>
inline void aframe(G& g, float cx, float cy, float u, float lean, uint16_t c) {
  float by = cy + 3.4f * u;
  g.wideLine(cx - 3.4f * u, by, cx + 3.4f * u, by, 0.38f * u, c);                 // tengely
  float tx = cx + lean * 2.6f * u, ty = cy - 2.6f * u;
  g.wideLine(cx - 2.0f * u, by, tx, ty, 0.36f * u, c);
  g.wideLine(cx + 2.0f * u, by, tx, ty, 0.36f * u, c);
  g.ring((int)tx, (int)(ty - 0.7f * u), (int)(0.75f * u), 0.4f * u, c);           // vonószem
}

template <class G>
inline void powerSym(G& g, float cx, float cy, float u, uint16_t c) {
  // be/ki szimbólum: nyitott kör + függőleges vonal
  const int N = 12;
  float r = 2.2f * u;
  float a0 = -60, a1 = 240;
  float px = cx + r * cosf(a0 * DEG), py = cy + r * sinf(a0 * DEG);
  for (int i = 1; i <= N; i++) {
    float a = a0 + (a1 - a0) * i / N;
    float x = cx + r * cosf(a * DEG), y = cy + r * sinf(a * DEG);
    g.wideLine(px, py, x, y, 0.42f * u, c);
    px = x; py = y;
  }
  g.wideLine(cx, cy - 2.9f * u, cx, cy - 0.6f * u, 0.42f * u, c);
}

// kis "AUTO" címke zöld háttéren
template <class G>
inline void autoTag(G& g, float cx, float cy, float u, const MIconColors& k) {
  g.fillRect((int)(cx - 3.2f * u), (int)(cy - 1.2f * u), (int)(6.4f * u), (int)(2.4f * u), k.accent);
  g.text("AUTO", (int)cx, (int)cy, 5.0f, k.fg, k.accent);
}

// két keretszakasz felül (a szakaszvezérlő ikonokhoz)
template <class G>
inline void sections(G& g, float cx, float cy, float u, uint16_t c) {
  g.wideLine(cx - 4.6f * u, cy, cx - 0.6f * u, cy, 0.42f * u, c);
  g.wideLine(cx + 0.6f * u, cy, cx + 4.6f * u, cy, 0.42f * u, c);
}

}  // namespace micon

// Ikon rajzolása (cx, cy) középpel, s méretű négyzetbe.
template <class G>
inline void drawMullerIcon(G& g, MIcon ic, int cx, int cy, int s, const MIconColors& k) {
  using namespace micon;
  float u = s / 10.0f;
  switch (ic) {
    case MIcon::None:
      break;

    // ---- permetezés (JB3: szakaszok felül, alatta nyíl + szórófej) ----
    case MIcon::Target100:
      nozzle(g, cx - 2.8f * u, cy + 0.4f * u, u, true, k);
      g.text("100%", (int)(cx + 2.4f * u), cy, u, k.fg, k.bg);
      break;
    case MIcon::SectMain:
      nozzle(g, cx - 2.6f * u, cy + 0.4f * u, u, true, k);
      powerSym(g, cx + 2.6f * u, cy + 0.6f * u, u, k.fg);
      break;
    case MIcon::SectOnRL:       // szakaszok bekapcsolása jobbról balra
      sections(g, cx, cy - 3.6f * u, u, k.fg);
      arrow(g, cx + 0.2f * u, cy + 0.8f * u, cx - 4.8f * u, cy + 0.8f * u, u, k.arrow);
      nozzle(g, cx + 2.8f * u, cy + 1.6f * u, u * 0.85f, true, k);
      break;
    case MIcon::SectOnLR:
      sections(g, cx, cy - 3.6f * u, u, k.fg);
      nozzle(g, cx - 2.8f * u, cy + 1.6f * u, u * 0.85f, true, k);
      arrow(g, cx - 0.2f * u, cy + 0.8f * u, cx + 4.8f * u, cy + 0.8f * u, u, k.arrow);
      break;
    case MIcon::SectOffLR:      // kikapcsolás balról jobbra
      sections(g, cx, cy - 3.6f * u, u, k.fg);
      nozzle(g, cx - 2.8f * u, cy + 1.6f * u, u * 0.85f, false, k);
      arrow(g, cx - 0.2f * u, cy + 0.8f * u, cx + 4.8f * u, cy + 0.8f * u, u, k.arrow);
      break;
    case MIcon::SectOffRL:
      sections(g, cx, cy - 3.6f * u, u, k.fg);
      arrow(g, cx + 0.2f * u, cy + 0.8f * u, cx - 4.8f * u, cy + 0.8f * u, u, k.arrow);
      nozzle(g, cx + 2.8f * u, cy + 1.6f * u, u * 0.85f, false, k);
      break;
    case MIcon::RateMinus:
      nozzle(g, cx - 3.0f * u, cy + 0.4f * u, u, true, k);
      g.text("-10%", (int)(cx + 2.2f * u), cy, u, k.arrow, k.bg);
      break;
    case MIcon::RatePlus:
      nozzle(g, cx - 3.0f * u, cy + 0.4f * u, u, true, k);
      g.text("+10%", (int)(cx + 2.2f * u), cy, u, k.arrow, k.bg);
      break;

    // ---- keret (rácsos keret + zöld nyilak) ----
    case MIcon::BoomAuto:
      autoTag(g, cx - 1.4f * u, cy - 3.6f * u, u, k);
      truss(g, cx, cy + 1.2f * u, u, 0, k.fg);
      g.text("MANU", (int)(cx + 1.4f * u), (int)(cy + 3.8f * u), 5.0f, k.fg, k.bg);
      break;
    case MIcon::BoomUp:
      truss(g, cx, cy + 3.4f * u, u, 0, k.fg);
      arrow(g, cx, cy + 0.6f * u, cx, cy - 4.4f * u, u, k.arrow);
      break;
    case MIcon::BoomDown:
      truss(g, cx, cy + 3.4f * u, u, 0, k.fg);
      arrow(g, cx, cy - 4.4f * u, cx, cy + 0.4f * u, u, k.arrow);
      break;
    case MIcon::FoldIn:         // szárnyak befelé: ívelt nyilak a két vég fölött, a közép felé
      truss(g, cx, cy + 3.4f * u, u, 0, k.fg);
      curvedArrow(g, cx - 2.2f * u, cy + 0.2f * u, 2.4f * u, 180, 300, u, k.arrow);
      curvedArrow(g, cx + 2.2f * u, cy + 0.2f * u, 2.4f * u, 0, -120, u, k.arrow);
      break;
    case MIcon::FoldOut:        // szárnyak kifelé: ívelt nyilak a közép felől kifelé, lefelé
      truss(g, cx, cy + 3.4f * u, u, 0, k.fg);
      curvedArrow(g, cx - 2.2f * u, cy + 0.2f * u, 2.4f * u, 300, 180, u, k.arrow);
      curvedArrow(g, cx + 2.2f * u, cy + 0.2f * u, 2.4f * u, -120, 0, u, k.arrow);
      break;
    case MIcon::TiltLeftUp:     // bal vége fel, jobb vége le
      truss(g, cx + 1.0f * u, cy + 2.6f * u, u, 14, k.fg, 8.0f);
      curvedArrow(g, cx - 2.6f * u, cy - 0.2f * u, 2.0f * u, 100, 250, u, k.arrow);
      break;
    case MIcon::TiltRightUp:    // jobb vége fel, bal vége le
      truss(g, cx - 1.0f * u, cy + 2.6f * u, u, -14, k.fg, 8.0f);
      curvedArrow(g, cx + 2.6f * u, cy - 0.2f * u, 2.0f * u, 80, -70, u, k.arrow);
      break;

    // ---- TRAIL (A-keret vonórúd) ----
    case MIcon::TrailAuto:
      autoTag(g, cx - 1.4f * u, cy - 3.6f * u, u, k);
      aframe(g, cx, cy + 0.6f * u, u * 0.75f, 0, k.fg);
      g.text("MANU", (int)(cx + 1.4f * u), (int)(cy + 4.0f * u), 5.0f, k.fg, k.bg);
      break;
    case MIcon::TrailOnOff:
      aframe(g, cx - 2.2f * u, cy, u * 0.85f, 0, k.fg);
      powerSym(g, cx + 2.6f * u, cy + 1.4f * u, u * 0.85f, k.fg);
      break;
    case MIcon::TrailCenter:    // -> o <- : vonórúd középre
      aframe(g, cx, cy + 0.4f * u, u * 0.9f, 0, k.fg);
      arrow(g, cx - 5.0f * u, cy - 2.4f * u, cx - 1.4f * u, cy - 2.4f * u, u, k.arrow);
      arrow(g, cx + 5.0f * u, cy - 2.4f * u, cx + 1.4f * u, cy - 2.4f * u, u, k.arrow);
      break;
    case MIcon::SteerLeft:      // rúd balra dől, nyíl a tengelynél balra
      aframe(g, cx, cy - 0.4f * u, u * 0.9f, -1, k.fg);
      arrow(g, cx + 4.6f * u, cy + 3.6f * u, cx - 0.4f * u, cy + 3.6f * u, u, k.arrow);
      break;
    case MIcon::SteerRight:
      aframe(g, cx, cy - 0.4f * u, u * 0.9f, 1, k.fg);
      arrow(g, cx - 4.6f * u, cy + 3.6f * u, cx + 0.4f * u, cy + 3.6f * u, u, k.arrow);
      break;
    case MIcon::AxleIn:         // két A-keret összekötve, nyilak befelé (gyári JB3 tandem ikon)
    case MIcon::AxleOut: {
      for (int side = -1; side <= 1; side += 2) {
        float ax = cx + side * 2.8f * u, ay = cy + 3.2f * u;
        g.wideLine(ax - 1.6f * u, ay, ax, cy - 0.4f * u, 0.32f * u, k.fg);
        g.wideLine(ax + 1.6f * u, ay, ax, cy - 0.4f * u, 0.32f * u, k.fg);
      }
      g.wideLine(cx - 1.2f * u, cy + 3.2f * u, cx + 1.2f * u, cy + 3.2f * u, 0.4f * u, k.fg);
      if (ic == MIcon::AxleIn) {
        arrow(g, cx - 4.8f * u, cy - 3.2f * u, cx - 0.8f * u, cy - 3.2f * u, u * 0.85f, k.arrow);
        arrow(g, cx + 4.8f * u, cy - 3.2f * u, cx + 0.8f * u, cy - 3.2f * u, u * 0.85f, k.arrow);
      } else {
        arrow(g, cx - 0.6f * u, cy - 3.2f * u, cx - 4.8f * u, cy - 3.2f * u, u * 0.85f, k.arrow);
        arrow(g, cx + 0.6f * u, cy - 3.2f * u, cx + 4.8f * u, cy - 3.2f * u, u * 0.85f, k.arrow);
      }
      break;
    }
    case MIcon::AxleLock: {     // lakat + kerékpár
      g.fillRect((int)(cx - 2.2f * u), (int)(cy - 1.0f * u), (int)(4.4f * u), (int)(3.2f * u), k.fg);
      g.fillRect((int)(cx - 0.35f * u), (int)(cy + 0.0f * u), (int)(0.7f * u), (int)(1.2f * u), k.bg);
      float r = 1.4f * u, px = cx - r, py = cy - 1.0f * u;
      for (int i = 1; i <= 8; i++) {   // kengyel
        float a = (180 + 180.0f * i / 8) * DEG;
        float x = cx + r * cosf(a), y = (cy - 1.0f * u) + r * sinf(a);
        g.wideLine(px, py, x, y, 0.4f * u, k.fg);
        px = x; py = y;
      }
      g.fillRect((int)(cx - 4.8f * u), (int)(cy - 0.6f * u), (int)(1.4f * u), (int)(3.6f * u), k.fg);
      g.fillRect((int)(cx + 3.4f * u), (int)(cy - 0.6f * u), (int)(1.4f * u), (int)(3.6f * u), k.fg);
      g.wideLine(cx - 3.4f * u, cy + 3.6f * u, cx + 3.4f * u, cy + 3.6f * u, 0.35f * u, k.fg);
      break;
    }
  }
}
