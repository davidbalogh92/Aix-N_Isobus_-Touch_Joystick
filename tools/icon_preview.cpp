// Müller képernyő előnézet PC-n: ugyanazt a muller_icons.h / keys.cpp kódot használja, mint a firmware.
// Fordítás és futtatás (a repó gyökeréből):
//   g++ -std=c++17 -Isrc tools/icon_preview.cpp src/keys.cpp -o /tmp/prev && /tmp/prev > /tmp/prev.txt
//   python3 tools/icon_preview.py /tmp/prev.txt docs/muller_elonezet.png
// Kimenet: PPM képek (P3 helyett nyers RGB hex) + szöveg parancsok, a python rakja össze PNG-vé.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include "keys.h"
#include "muller_funcs.h"
#include "muller_layout.h"
#include "muller_icons.h"

#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

struct Mock {
  int W, H;
  std::vector<uint16_t> px;
  std::vector<std::string> texts;
  Mock(int w, int h) : W(w), H(h), px(w * h, 0) {}
  void set(int x, int y, uint16_t c) { if (x >= 0 && y >= 0 && x < W && y < H) px[y * W + x] = c; }
  void fillRect(int x, int y, int w, int h, uint16_t c) {
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) set(i, j, c);
  }
  void wideLine(float x0, float y0, float x1, float y1, float r, uint16_t c) {
    int minx = (int)floorf(fminf(x0, x1) - r - 1), maxx = (int)ceilf(fmaxf(x0, x1) + r + 1);
    int miny = (int)floorf(fminf(y0, y1) - r - 1), maxy = (int)ceilf(fmaxf(y0, y1) + r + 1);
    float dx = x1 - x0, dy = y1 - y0, L2 = dx * dx + dy * dy;
    for (int y = miny; y <= maxy; y++)
      for (int x = minx; x <= maxx; x++) {
        float t = L2 > 0 ? ((x - x0) * dx + (y - y0) * dy) / L2 : 0;
        t = fmaxf(0, fminf(1, t));
        float ex = x0 + t * dx - x, ey = y0 + t * dy - y;
        if (ex * ex + ey * ey <= r * r) set(x, y, c);
      }
  }
  void fillTri(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    int minx = std::min(x0, std::min(x1, x2)), maxx = std::max(x0, std::max(x1, x2));
    int miny = std::min(y0, std::min(y1, y2)), maxy = std::max(y0, std::max(y1, y2));
    auto e = [](int ax, int ay, int bx, int by, int px, int py) { return (bx - ax) * (py - ay) - (by - ay) * (px - ax); };
    for (int y = miny; y <= maxy; y++)
      for (int x = minx; x <= maxx; x++) {
        int a = e(x0, y0, x1, y1, x, y), b = e(x1, y1, x2, y2, x, y), d = e(x2, y2, x0, y0, x, y);
        if ((a >= 0 && b >= 0 && d >= 0) || (a <= 0 && b <= 0 && d <= 0)) set(x, y, c);
      }
  }
  void ring(int cx, int cy, int r, float th, uint16_t c) {
    for (int y = cy - r - 3; y <= cy + r + 3; y++)
      for (int x = cx - r - 3; x <= cx + r + 3; x++) {
        float d = sqrtf((float)(x - cx) * (x - cx) + (float)(y - cy) * (y - cy));
        if (fabsf(d - r) <= th) set(x, y, c);
      }
  }
  void text(const char* s, int cx, int cy, float u, uint16_t fg, uint16_t bg) {
    char b[160];
    snprintf(b, sizeof(b), "T %d %d %d %04X %04X %s", cx, cy, u < 7 ? 13 : 16, fg, bg, s);
    texts.push_back(b);
  }
  void textSz(const char* s, int cx, int cy, int sz, uint16_t fg, uint16_t bg) {
    char b[160];
    snprintf(b, sizeof(b), "T %d %d %d %04X %04X %s", cx, cy, sz, fg, bg, s);
    texts.push_back(b);
  }
  void roundRect(int x, int y, int w, int h, int r, uint16_t c) {
    for (int j = y; j < y + h; j++)
      for (int i = x; i < x + w; i++) {
        int dx = (i < x + r) ? x + r - i : (i >= x + w - r ? i - (x + w - r - 1) : 0);
        int dy = (j < y + r) ? y + r - j : (j >= y + h - r ? j - (y + h - r - 1) : 0);
        if (dx * dx + dy * dy <= r * r) set(i, j, c);
      }
  }
};

// a firmware színei (main.cpp-vel egyezően) - JB3 stílus
static const uint16_t C_BG = RGB565(16, 18, 22), C_BAR = RGB565(36, 40, 48);
static const uint16_t C_MKEY = RGB565(74, 76, 80), C_MKEY_EDGE = RGB565(120, 124, 130);
static const uint16_t C_MFREE = RGB565(44, 46, 50), C_MPRESS = RGB565(255, 150, 0);
static const uint16_t C_TEXT = RGB565(235, 235, 235), C_DIM = RGB565(150, 155, 165);
static const uint16_t C_WHITE = RGB565(245, 245, 245);
static const uint16_t LAYER_ON[3]  = {RGB565(230, 40, 40), RGB565(245, 200, 30), RGB565(70, 190, 70)};
static const uint16_t LAYER_DIM[3] = {RGB565(90, 30, 30), RGB565(95, 80, 20), RGB565(30, 80, 35)};

static MIconColors kOn() { return MIconColors{C_WHITE, RGB565(30, 140, 235), RGB565(80, 210, 70), C_MKEY, RGB565(60, 170, 60)}; }

static void drawSlot(Mock& g, const KeyRect& r, uint8_t code, const char* corner, bool pressed, bool editor) {
  bool act = slotActive(code);
  uint16_t fill = !act ? C_MFREE : (pressed ? C_MPRESS : C_MKEY);
  g.roundRect(r.x, r.y, r.w, r.h, 10, act ? C_MKEY_EDGE : C_MFREE);
  g.roundRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, 9, fill);
  if (editor && !act) {   // szerkesztőben üres hely: + jel
    g.wideLine(r.x + r.w / 2 - 10, r.y + r.h / 2, r.x + r.w / 2 + 10, r.y + r.h / 2, 2.5f, C_DIM);
    g.wideLine(r.x + r.w / 2, r.y + r.h / 2 - 10, r.x + r.w / 2, r.y + r.h / 2 + 10, 2.5f, C_DIM);
  }
  bool label = act && r.h >= 70;
  int iconH = r.h - (label ? 18 : 0);
  int s = (int)(std::min((int)r.w, iconH) * 0.74f);
  MIconColors k = kOn(); k.bg = fill;
  if (act) drawMullerIcon(g, slotIcon(code), r.x + r.w / 2, r.y + iconH / 2 + (label ? 3 : 0), s, k);
  if (label) g.textSz(slotLabel(code, true), r.x + r.w / 2, r.y + r.h - 11, 13, C_TEXT, fill);
  if (corner) g.textSz(corner, r.x + 14, r.y + 11, 11, act ? RGB565(170, 172, 178) : RGB565(90, 92, 98), fill);
}

static void emit(Mock& g) {
  printf("IMG %d %d\n", g.W, g.H);
  for (int i = 0; i < g.W * g.H; i++) printf("%04X", g.px[i]);
  printf("\n");
  for (auto& t : g.texts) printf("%s\n", t.c_str());
  printf("END\n");
}

static void drawTabsP(Mock& g, int W, int statusH, int tabH, const uint8_t names[3], const bool vis[3], int sel) {
  int idx[3], nv = 0;
  for (int t = 0; t < 3; t++) if (vis[t]) idx[nv++] = t;
  int tw = (W - (nv + 1) * 6) / nv;
  for (int k = 0; k < nv; k++) {
    int t = idx[k], x = 6 + k * (tw + 6);
    bool on = (t == sel);
    uint16_t c = on ? LAYER_ON[t] : LAYER_DIM[t];
    g.roundRect(x, statusH + 4, tw, tabH - 8, 8, c);
    g.textSz(tabName(names[t], true), x + tw / 2, statusH + tabH / 2, tw < 130 ? 13 : 16,
             on ? (t == 1 ? RGB565(0, 0, 0) : C_WHITE) : C_DIM, c);
  }
}

// főképernyő vagy szerkesztő
static void renderLayout(int W, int H, uint8_t slots[3][8], uint8_t names[3], int tab, int pressed, bool editor) {
  const int statusH = 30, tabH = 46;
  Mock g(W, H);
  g.fillRect(0, 0, W, H, C_BG);
  g.fillRect(0, 0, W, statusH, C_BAR);
  if (editor) {
    g.textSz("Edit layout", 60, statusH / 2, 16, C_TEXT, C_BAR);
    g.roundRect(W - 88, 2, 84, statusH - 4, 6, RGB565(40, 110, 60));
    g.textSz("Done", W - 46, statusH / 2, 16, C_WHITE, RGB565(40, 110, 60));
  } else {
    g.textSz("Müller   CAN: OK   addr 0x90", W / 2, statusH / 2, 16, C_TEXT, C_BAR);
  }
  bool vis[3];
  for (int t = 0; t < 3; t++) vis[t] = editor || layoutTabVisible(slots, t);
  drawTabsP(g, W, statusH, tabH, names, vis, tab);
  KeyRect r[8];
  int bottom = editor ? 22 : 0;
  mullerLayout(0, statusH + tabH, W, H - statusH - tabH - bottom, r);
  for (int b = 0; b < 8; b++) {
    char nb[4]; snprintf(nb, 4, "B%d", b + 1);
    drawSlot(g, r[b], slots[tab][b], nb, b == pressed, editor);
  }
  int dotx = (r[2].x + r[2].w + r[3].x) / 2, doty = (r[2].y + r[2].h + r[4].y) / 2;
  for (int y = doty - 7; y <= doty + 7; y++)
    for (int x = dotx - 7; x <= dotx + 7; x++)
      if ((x - dotx) * (x - dotx) + (y - doty) * (y - doty) <= 49) g.set(x, y, LAYER_ON[tab]);
  if (editor) g.textSz("Tap slot: choose   Tap tab again: rename", W / 2, H - 11, 11, C_DIM, C_BG);
  emit(g);
}

// funkcióválasztó
static void renderPicker(int W, int H, int group, uint8_t current, int slotB, uint8_t tabNameIdx) {
  const int statusH = 30, tabH = 46;
  Mock g(W, H);
  g.fillRect(0, 0, W, H, C_BG);
  g.fillRect(0, 0, W, statusH, C_BAR);
  char t[64]; snprintf(t, 64, "Function for %s B%d", tabName(tabNameIdx, true), slotB);
  g.textSz(t, W / 2, statusH / 2, 16, C_TEXT, C_BAR);
  uint8_t nm[3] = {0, 1, 2};
  bool vis[3] = {true, true, true};
  drawTabsP(g, W, statusH, tabH, nm, vis, group);
  uint8_t items[12];
  uint8_t n = pickerItems(group, items);
  int bh = 52;
  int cols = W > H ? 4 : 3, rows = W > H ? 3 : 4;
  KeyRect r[12];
  gridLayout(0, statusH + tabH, W, H - statusH - tabH - bh - 8, cols, rows, r);
  for (int i = 0; i < n; i++) {
    bool cur = (items[i] == current);
    drawSlot(g, r[i], items[i], nullptr, false, false);
    if (cur) {  // jelenlegi: narancs keret
      for (int k = 0; k < 3; k++) {
        g.wideLine(r[i].x + k, r[i].y + k, r[i].x + r[i].w - k, r[i].y + k, 0.6f, C_MPRESS);
        g.wideLine(r[i].x + k, r[i].y + r[i].h - k, r[i].x + r[i].w - k, r[i].y + r[i].h - k, 0.6f, C_MPRESS);
        g.wideLine(r[i].x + k, r[i].y + k, r[i].x + k, r[i].y + r[i].h - k, 0.6f, C_MPRESS);
        g.wideLine(r[i].x + r[i].w - k, r[i].y + k, r[i].x + r[i].w - k, r[i].y + r[i].h - k, 0.6f, C_MPRESS);
      }
    }
  }
  int bw = (W - 3 * 6) / 2, by = H - bh - 6;
  g.roundRect(6, by, bw, bh, 8, RGB565(70, 50, 50));
  g.textSz("Empty", 6 + bw / 2, by + bh / 2, 16, C_WHITE, RGB565(70, 50, 50));
  g.roundRect(12 + bw, by, bw, bh, 8, RGB565(45, 65, 100));
  g.textSz("Cancel", 12 + bw + bw / 2, by + bh / 2, 16, C_WHITE, RGB565(45, 65, 100));
  emit(g);
}

int main() {
  uint8_t sl[3][8], nm[3];
  layoutDefault(sl, nm);
  // példa kevert kiosztás: 1. fül "MAIN": a leggyakoribb funkciók
  uint8_t mix[8] = {8, 9, 18, 20, 19, 21, 14, 15};   // célérték, főkapcs., emel, behajt, süllyeszt, kihajt, adag-, adag+
  for (int b = 0; b < 8; b++) sl[0][b] = mix[b];
  nm[0] = 6;   // MAIN
  sl[2][1] = 3; // keret fülre a "középre" is
  nm[2] = 7;   // EXTRA
  renderLayout(320, 480, sl, nm, 0, 2, false);
  renderLayout(320, 480, sl, nm, 2, -1, false);
  renderLayout(320, 480, sl, nm, 0, -1, true);
  renderPicker(320, 480, 0, 3, 2, 7);
  renderPicker(320, 480, 1, 9, 2, 6);
  return 0;
}
