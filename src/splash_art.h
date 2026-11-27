#pragma once
// Nyitókép grafika: "mindenhez kapcsolódik" - középen a Touch Joystick, körülötte a munkagépek és a terminál,
// ISOBUS-vonalakkal összekötve. Sablon a rajzoló G típusra (firmware: LovyanGFX adapter, PC: előnézet).
//
// G-nek a muller_icons.h primitívjei kellenek, plusz:
//   void fillCircle(int x, int y, int r, uint16_t c);
//   void roundRect(int x, int y, int w, int h, int r, uint16_t c);   // kitöltött lekerekített téglalap
#include <math.h>
#include "muller_icons.h"

struct SplashColors {
  uint16_t bg, line, node, nodeEdge, fg, blue, green, orange, red, yellow;
};

namespace splashart {

template <class G>
inline void seeder(G& g, float cx, float cy, float u, uint16_t c) {
  // vetőgép: tartály (trapéz) + csoroszlyák
  g.fillTri((int)(cx - 4 * u), (int)(cy - 3 * u), (int)(cx + 4 * u), (int)(cy - 3 * u), (int)(cx + 2.6f * u), (int)(cy + 0.4f * u), c);
  g.fillTri((int)(cx - 4 * u), (int)(cy - 3 * u), (int)(cx + 2.6f * u), (int)(cy + 0.4f * u), (int)(cx - 2.6f * u), (int)(cy + 0.4f * u), c);
  g.wideLine(cx - 4.4f * u, cy + 1.4f * u, cx + 4.4f * u, cy + 1.4f * u, 0.35f * u, c);
  for (int i = -2; i <= 2; i++) g.ring((int)(cx + i * 1.8f * u), (int)(cy + 3.0f * u), (int)(0.8f * u), 0.3f * u, c);
}

template <class G>
inline void spreader(G& g, float cx, float cy, float u, uint16_t c, uint16_t accent) {
  // szóró: tartály + két szórótányér, szórt szemcsék
  g.fillTri((int)(cx - 3.6f * u), (int)(cy - 3.6f * u), (int)(cx + 3.6f * u), (int)(cy - 3.6f * u), (int)(cx + 1.2f * u), (int)(cy + 0.4f * u), c);
  g.fillTri((int)(cx - 3.6f * u), (int)(cy - 3.6f * u), (int)(cx + 1.2f * u), (int)(cy + 0.4f * u), (int)(cx - 1.2f * u), (int)(cy + 0.4f * u), c);
  g.wideLine(cx - 2.6f * u, cy + 1.6f * u, cx - 0.6f * u, cy + 1.6f * u, 0.45f * u, c);
  g.wideLine(cx + 0.6f * u, cy + 1.6f * u, cx + 2.6f * u, cy + 1.6f * u, 0.45f * u, c);
  for (int i = 0; i < 5; i++) {
    float a = (200 + i * 35) * micon::DEG;
    g.fillCircle((int)(cx - 1.6f * u + cosf(a) * 3.4f * u), (int)(cy + 2.4f * u - sinf(a) * 1.6f * u), (int)(0.35f * u + 1), accent);
    float b = (-20 - i * 35) * micon::DEG;
    g.fillCircle((int)(cx + 1.6f * u + cosf(b) * 3.4f * u), (int)(cy + 2.4f * u - sinf(b) * 1.6f * u), (int)(0.35f * u + 1), accent);
  }
}

template <class G>
inline void terminal(G& g, float cx, float cy, float u, uint16_t c, uint16_t screen) {
  g.roundRect((int)(cx - 4 * u), (int)(cy - 3 * u), (int)(8 * u), (int)(6 * u), (int)(0.8f * u), c);
  g.fillRect((int)(cx - 3.2f * u), (int)(cy - 2.3f * u), (int)(5.0f * u), (int)(4.6f * u), screen);
  for (int i = 0; i < 4; i++) g.fillRect((int)(cx + 2.2f * u), (int)(cy - 2.2f * u + i * 1.2f * u), (int)(0.9f * u), (int)(0.8f * u), screen);
}

template <class G>
inline void plug(G& g, float cx, float cy, float u, uint16_t c, uint16_t bg) {
  // ISOBUS csatlakozó (kör, tűk)
  g.fillCircle((int)cx, (int)cy, (int)(3.6f * u), c);
  g.fillCircle((int)cx, (int)cy, (int)(2.8f * u), bg);
  for (int i = 0; i < 8; i++) {
    float a = i * 45 * micon::DEG;
    g.fillCircle((int)(cx + cosf(a) * 1.8f * u), (int)(cy + sinf(a) * 1.8f * u), (int)(0.4f * u + 1), c);
  }
  g.fillCircle((int)cx, (int)cy, (int)(0.5f * u + 1), c);
  g.fillRect((int)(cx - 0.8f * u), (int)(cy - 4.6f * u), (int)(1.6f * u), (int)(1.2f * u), c);
}

// a Touch Joystick maga: kis kijelző a 8 gombbal (joystick elrendezés), színes ponttal
template <class G>
inline void joystick(G& g, float cx, float cy, float u, const SplashColors& k) {
  float w = 11 * u, h = 13 * u;
  g.roundRect((int)(cx - w / 2 - u), (int)(cy - h / 2 - u), (int)(w + 2 * u), (int)(h + 2 * u), (int)(1.4f * u), k.nodeEdge);
  g.roundRect((int)(cx - w / 2), (int)(cy - h / 2), (int)w, (int)h, (int)(1.0f * u), k.node);
  // 3 fül
  uint16_t tc[3] = {k.red, k.yellow, k.green};
  for (int i = 0; i < 3; i++)
    g.roundRect((int)(cx - w / 2 + 0.6f * u + i * 3.4f * u), (int)(cy - h / 2 + 0.6f * u), (int)(3.0f * u), (int)(1.4f * u), (int)(0.4f * u), tc[i]);
  // gombok: B1 | B3 B4 / B2 | B5 B6 / . | B7 B8
  float gx[3] = {cx - 3.8f * u, cx - 0.3f * u, cx + 3.1f * u}, gy[3] = {cy - 2.6f * u, cy + 0.9f * u, cy + 4.4f * u};
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++) {
      if (c == 0 && r == 2) continue;
      bool hot = (r == 0 && c == 1);
      g.roundRect((int)(gx[c] - 1.4f * u), (int)(gy[r] - 1.4f * u), (int)(2.8f * u), (int)(2.8f * u), (int)(0.5f * u), hot ? k.orange : k.fg);
    }
  g.fillCircle((int)(cx + 1.4f * u), (int)(cy - 0.85f * u), (int)(0.45f * u + 1), k.green);
}

}  // namespace splashart

// A teljes kompozíció: középpont (cx, cy), R = a csomópontok köre
template <class G>
inline void drawSplashArt(G& g, int cx, int cy, int R, const SplashColors& k) {
  using namespace splashart;
  const int N = 6;
  const float nodeR = R * 0.27f;
  float u = nodeR / 6.0f;
  MIconColors ic{k.fg, k.blue, k.green, k.node, k.green};
  float nx[N], ny[N];
  for (int i = 0; i < N; i++) {
    float a = (-90 + i * 60) * micon::DEG;
    nx[i] = cx + cosf(a) * R;
    ny[i] = cy + sinf(a) * R * 0.92f;
  }
  // vonalak: központ -> csomópontok (ISOBUS), és a gyűrű szakaszai
  for (int i = 0; i < N; i++) {
    g.wideLine((float)cx, (float)cy, nx[i], ny[i], 2.0f, k.line);
    int j = (i + 1) % N;
    g.wideLine(nx[i], ny[i], nx[j], ny[j], 1.0f, k.line);
  }
  // kis "adatcsomag" pontok a vonalakon
  for (int i = 0; i < N; i++) {
    float t = 0.42f + 0.12f * (i % 3);
    g.fillCircle((int)(cx + (nx[i] - cx) * t), (int)(cy + (ny[i] - cy) * t), 4, i % 2 ? k.green : k.orange);
  }
  // csomópontok
  for (int i = 0; i < N; i++) {
    g.fillCircle((int)nx[i], (int)ny[i], (int)nodeR + 2, k.nodeEdge);
    g.fillCircle((int)nx[i], (int)ny[i], (int)nodeR, k.node);
  }
  // 0 fent: permetező keret + szórófej
  micon::truss(g, nx[0], ny[0] + 1.8f * u, u * 0.9f, 0, k.fg);
  micon::nozzle(g, nx[0], ny[0] - 1.2f * u, u * 0.55f, true, ic);
  // 1 jobbra fent: vetőgép
  seeder(g, nx[1], ny[1], u * 0.9f, k.fg);
  // 2 jobbra lent: szóró
  spreader(g, nx[2], ny[2], u * 0.9f, k.fg, k.orange);
  // 3 lent: ISOBUS csatlakozó
  plug(g, nx[3], ny[3], u * 0.95f, k.fg, k.node);
  // 4 balra lent: TRAIL vonórúd
  micon::aframe(g, nx[4], ny[4], u * 0.85f, 0.6f, k.fg);
  // 5 balra fent: terminál
  terminal(g, nx[5], ny[5], u * 0.9f, k.fg, k.blue);
  // közép: a joystick
  joystick(g, (float)cx, (float)cy, R * 0.048f, k);
}
