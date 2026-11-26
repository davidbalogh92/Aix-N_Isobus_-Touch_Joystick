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

// a firmware színei (main.cpp-vel egyezően)
static const uint16_t C_BG = RGB565(16, 18, 22), C_BAR = RGB565(36, 40, 48);
static const uint16_t C_MKEY = RGB565(228, 230, 234), C_MFREE = RGB565(58, 62, 72);
static const uint16_t C_TAB = RGB565(52, 58, 70), C_TAB_ON = RGB565(255, 150, 0);
static const uint16_t C_TEXT = RGB565(235, 235, 235), C_DIM = RGB565(150, 155, 165);

static void renderScreen(int W, int H, int statusH, int tabH, uint8_t layer, int pressedBtn) {
  Mock g(W, H);
  g.fillRect(0, 0, W, H, C_BG);
  g.fillRect(0, 0, W, statusH, C_BAR);
  g.textSz("Müller   CAN: OK   cím 0x90", W / 2, statusH / 2, 16, C_TEXT, C_BAR);
  // fülek
  int tw = (W - 4 * 6) / 3;
  for (int i = 0; i < 3; i++) {
    int x = 6 + i * (tw + 6);
    uint16_t c = (i == layer) ? C_TAB_ON : C_TAB;
    g.roundRect(x, statusH + 4, tw, tabH - 8, 8, c);
    g.textSz(mullerLayerName(i), x + tw / 2, statusH + tabH / 2, tw < 130 ? 13 : 16, i == layer ? 0 : C_TEXT, c);
  }
  KeyRect r[8];
  mullerLayout(0, statusH + tabH, W, H - statusH - tabH, r);
  MIconColors k{RGB565(20, 20, 24), RGB565(0, 110, 230), RGB565(225, 30, 30), C_MKEY};
  for (int b = 0; b < 8; b++) {
    uint8_t in = mullerInputOf(layer, b + 1);
    const MFunc& f = mullerFunc(in);
    bool asg = mullerAssigned(in);
    uint16_t fill = !asg ? C_MFREE : (b == pressedBtn ? C_TAB_ON : C_MKEY);
    g.roundRect(r[b].x, r[b].y, r[b].w, r[b].h, 10, fill);
    k.bg = fill;
    k.fg = asg ? RGB565(20, 20, 24) : C_DIM;
    bool label = asg && r[b].h >= 80;
    int iconH = r[b].h - (label ? 18 : 0);
    int s = (int)(std::min((int)r[b].w, iconH) * 0.78f);
    drawMullerIcon(g, f.icon, r[b].x + r[b].w / 2, r[b].y + iconH / 2 + (label ? 2 : 0), s, k);
    if (label) g.textSz(f.label, r[b].x + r[b].w / 2, r[b].y + r[b].h - 11, 13, RGB565(20, 20, 24), fill);
    char nb[4]; snprintf(nb, 4, "B%d", b + 1);
    g.textSz(nb, r[b].x + 14, r[b].y + 11, 12, asg ? RGB565(120, 120, 130) : C_DIM, fill);
  }
  printf("IMG %d %d\n", W, H);
  for (int i = 0; i < W * H; i++) printf("%04X", g.px[i]);
  printf("\n");
  for (auto& t : g.texts) printf("%s\n", t.c_str());
  printf("END\n");
}

int main() {
  // álló 3,5" (320x480): mindhárom fül
  for (int L = 0; L < 3; L++) renderScreen(320, 480, 30, 46, L, L == 2 ? 2 : -1);
  // fekvő 3,5" (480x320): permetezés fül
  renderScreen(480, 320, 30, 46, 1, -1);
  return 0;
}
