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
#include "muller_profile.h"
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

static void renderScreen(int W, int H, int statusH, int tabH, uint8_t layer, int pressedBtn,
                         const uint8_t* cfg, bool editor = false) {
  Mock g(W, H);
  g.fillRect(0, 0, W, H, C_BG);
  g.fillRect(0, 0, W, statusH, C_BAR);
  g.textSz(editor ? "Edit buttons        Towed   [Done]" : "Müller   CAN: OK   addr 0x90", W / 2, statusH / 2, 16, C_TEXT, C_BAR);
  int vis[3], nv = 0;
  for (int l = 0; l < 3; l++) if (editor || mullerLayerVisible(cfg, l)) vis[nv++] = l;
  int tw = (W - (nv + 1) * 6) / nv;
  for (int t = 0; t < nv; t++) {
    int i = vis[t];
    int x = 6 + t * (tw + 6);
    bool on = (i == layer);
    uint16_t c = on ? LAYER_ON[i] : LAYER_DIM[i];
    g.roundRect(x, statusH + 4, tw, tabH - 8, 8, c);
    g.textSz(mullerLayerName(i, true), x + tw / 2, statusH + tabH / 2, tw < 130 ? 13 : 16,
             on ? (i == 1 ? RGB565(0, 0, 0) : C_WHITE) : C_DIM, c);
  }
  KeyRect r[8];
  mullerLayout(0, statusH + tabH, W, H - statusH - tabH, r);
  // jobb blokk körvonal + középső színes pont (mint a joystickon)
  int bx0 = r[2].x - 4, by0 = r[2].y - 4, bx1 = r[3].x + r[3].w + 4, by1 = r[5].y + r[5].h + 4;
  int dotx = (r[2].x + r[2].w + r[3].x) / 2, doty = (r[2].y + r[2].h + r[4].y) / 2;
  (void)bx0; (void)by0; (void)bx1; (void)by1;
  MIconColors k{C_WHITE, RGB565(30, 140, 235), RGB565(80, 210, 70), C_MKEY, RGB565(60, 170, 60)};
  for (int b = 0; b < 8; b++) {
    uint8_t in = mullerInputOf(layer, b + 1);
    bool asg = mullerBtnActive(cfg, in);
    bool showIcon = asg || (editor && mullerAssigned(in));
    uint16_t fill = !asg ? C_MFREE : (b == pressedBtn ? C_MPRESS : C_MKEY);
    g.roundRect(r[b].x, r[b].y, r[b].w, r[b].h, 10, asg ? C_MKEY_EDGE : C_MFREE);
    g.roundRect(r[b].x + 2, r[b].y + 2, r[b].w - 4, r[b].h - 4, 9, fill);
    k.bg = fill;
    bool label = showIcon && r[b].h >= 80;
    int iconH = r[b].h - (label ? 18 : 0);
    int s = (int)(std::min((int)r[b].w, iconH) * 0.74f);
    MIconColors kk = asg ? k : MIconColors{RGB565(95, 98, 104), RGB565(60, 80, 110), RGB565(70, 100, 70), fill, RGB565(50, 70, 50)};
    kk.bg = fill;
    if (showIcon) drawMullerIcon(g, mullerIconFor(cfg, in), r[b].x + r[b].w / 2, r[b].y + iconH / 2 + (label ? 3 : 0), s, kk);
    if (label) g.textSz(asg ? mullerLabelFor(cfg, in, true) : "OFF", r[b].x + r[b].w / 2, r[b].y + r[b].h - 11, 13, asg ? C_TEXT : C_DIM, fill);
    char nb[4]; snprintf(nb, 4, "B%d", b + 1);
    g.textSz(nb, r[b].x + 14, r[b].y + 11, 11, asg ? RGB565(170, 172, 178) : RGB565(90, 92, 98), fill);
  }
  // színes pont a B3-B6 blokk közepén
  for (int y = doty - 7; y <= doty + 7; y++)
    for (int x = dotx - 7; x <= dotx + 7; x++)
      if ((x - dotx) * (x - dotx) + (y - doty) * (y - doty) <= 49) g.set(x, y, LAYER_ON[layer]);
  printf("IMG %d %d\n", W, H);
  for (int i = 0; i < W * H; i++) printf("%04X", g.px[i]);
  printf("\n");
  for (auto& t : g.texts) printf("%s\n", t.c_str());
  printf("END\n");
}

int main() {
  uint8_t own[24], jb3[24], self_[24];
  mullerApplyProfile(PROF_TOWED_OWN, own);
  mullerApplyProfile(PROF_TOWED_JB3, jb3);
  mullerApplyProfile(PROF_SELF, self_);
  // álló 3,5": saját vontatott profil mindhárom fül (TRAIL-en B5 nyomva), szerkesztő (TRAIL), önjáró
  renderScreen(320, 480, 30, 46, 0, 4, own);
  renderScreen(320, 480, 30, 46, 1, -1, own);
  renderScreen(320, 480, 30, 46, 2, -1, own);
  renderScreen(320, 480, 30, 46, 0, -1, own, true);
  renderScreen(320, 480, 30, 46, 0, -1, jb3);
  renderScreen(320, 480, 30, 46, 1, -1, self_);
  return 0;
}
