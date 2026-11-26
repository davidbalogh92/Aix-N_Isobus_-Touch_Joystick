// Natív (PC) tesztek a hardverfüggetlen részekhez.
// Futtatás:  g++ -std=c++17 -Isrc test/native_test.cpp src/keys.cpp src/muller_out.cpp -o /tmp/t && /tmp/t
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <initializer_list>
#include "keys.h"
#include "muller_out.h"
#include "muller_funcs.h"
#include "light_logic.h"
#include "muller_layout.h"

struct Frame { uint32_t id; uint8_t d[8]; uint8_t len; };
static std::vector<Frame> tx;
static const Frame& lastStatus() {   // utolsó 0x1CE6 (AUX-O állapot) keret
  for (size_t i = tx.size(); i-- > 0;) if ((tx[i].id & 0xFFFFFF00) == 0x1CE6FF00) return tx[i];
  assert(false); return tx[0];
}
static bool capture(uint32_t id, const uint8_t* d, uint8_t len) {
  Frame f{id, {0}, len}; memcpy(f.d, d, len); tx.push_back(f); return true;
}

static void test_layouts() {
  KeyRect r[8];
  for (int L : {3, 6, 8}) {
    assert(layoutCompute(L, 0, 30, 480, 290, r, 8) == L);
    assert(layoutCompute(L, 0, 30, 320, 450, r, 8) == L);
  }
  // álló: 8 gomb 2 oszlop
  layoutCompute(8, 0, 30, 320, 450, r, 8);
  assert(r[0].y == r[1].y && r[2].y > r[0].y);
  // holtsáv
  assert(layoutHitTest(r, 8, r[0].x + r[0].w / 2, r[0].y + r[0].h / 2, 6) == 0);
  assert(layoutHitTest(r, 8, r[0].x + 2, r[0].y + r[0].h / 2, 6) == -1);
  // Müller elrendezés
  for (auto wh : {std::make_pair(480, 246), std::make_pair(320, 406)}) {
    mullerLayout(0, 74, wh.first, wh.second, r);
    assert(r[0].x == r[1].x && r[0].y < r[1].y);           // B1 fölött B2
    assert(r[2].y == r[0].y && r[3].y == r[0].y);           // B3,B4 első sor
    assert(r[2].x - (r[0].x + r[0].w) > r[3].x - (r[2].x + r[2].w)); // szélesebb elválasztó
    assert(r[6].y > r[4].y && r[4].y > r[2].y);             // B3 / B5 / B7 oszlop
    for (int i = 0; i < 8; i++) assert(r[i].x + r[i].w <= wh.first && r[i].y + r[i].h <= 74 + wh.second);
    for (int i = 0; i < 8; i++)
      for (int j = i + 1; j < 8; j++)  // nincs átfedés
        assert(r[i].x + r[i].w <= r[j].x || r[j].x + r[j].w <= r[i].x ||
               r[i].y + r[i].h <= r[j].y || r[j].y + r[j].h <= r[i].y);
  }
}

static void test_touchfilter() {
  TouchFilter f({50, 30, 18, 30});
  uint32_t t;
  for (t = 0; t < 50; t += 10) assert(!f.update(true, 100, 100, t));
  assert(f.update(true, 101, 100, 50) && f.pressed());
  assert(!f.update(false, 0, 0, 60) && f.pressed());          // rövid kimaradás
  f.update(false, 0, 0, 70); f.update(false, 0, 0, 80);
  assert(!f.pressed());
  for (t = 200; t <= 250; t += 10) f.update(true, 50, 50, t);
  assert(f.pressed());
  assert(f.update(true, 150, 60, 260) && f.blocked());         // ugrás -> blokk
  for (t = 270; t <= 320; t += 10) f.update(false, 0, 0, t);
  assert(!f.blocked() && !f.pressed());
}

static void test_name() {
  assert(MullerAuxOSender::makeName(0x1ABCDE) == 0xA01A80000C5ABCDEULL);  // a bizonyítottan működő NAME
}

static void test_funcs() {
  // táblázat a docs/muller szerint
  // TRAIL a felhasználó gépe szerint: B5 = középre (tartásos), B4/B6 = keret össze/kinyitás, B3 = lakat (gyári)
  assert(mullerInputOf(0, 5) == 3 && mullerFunc(3).icon == MIcon::TrailCenter && mullerFunc(3).hold);
  assert(mullerInputOf(0, 4) == 4 && mullerFunc(4).icon == MIcon::FoldIn && mullerFunc(4).hold);
  assert(mullerInputOf(0, 6) == 5 && mullerFunc(5).icon == MIcon::FoldOut && mullerFunc(5).hold);
  assert(mullerInputOf(0, 3) == 2 && mullerFunc(2).icon == MIcon::AxleLock);
  assert(mullerInputOf(1, 4) == 12 && mullerFunc(12).icon == MIcon::SectOnLR);
  assert(mullerInputOf(1, 5) == 11 && mullerFunc(11).icon == MIcon::SectOffLR);
  assert(mullerInputOf(2, 3) == 18 && mullerFunc(18).icon == MIcon::BoomUp);
  assert(mullerInputOf(2, 5) == 19 && mullerFunc(19).icon == MIcon::BoomDown);
  assert(mullerInputOf(2, 8) == 23);
  assert(!mullerAssigned(mullerInputOf(2, 2)));
  assert(mullerInputOf(3, 1) == 0xFF && mullerInputOf(0, 9) == 0xFF);
  int assigned = 0;
  for (int i = 0; i < 24; i++) assigned += mullerAssigned(i);
  assert(assigned == 23);   // csak a keret B2 kiosztatlan
}

static void test_sender_basic() {
  tx.clear();
  MullerAuxOSender s; s.send = capture;
  s.begin(0x1ABCDE, 0x90, 1000);
  assert(tx.size() == 1 && tx[0].id == 0x18EEFF90);
  assert(tx[0].d[0] == 0xDE && tx[0].d[7] == 0xA0);           // NAME little endian
  // claim előtt nem küld
  assert(!s.press(9, 1100));
  assert(tx.size() == 1);
  s.update(1249); assert(!s.claimed());
  s.update(1250); assert(s.claimed());
  assert(tx.back().id == 0x1CE7FF90);                          // azonnali életjel
  size_t n0 = tx.size();
  // lenyomás
  assert(s.press(18, 2000));
  assert(tx.size() == n0 + 1);
  const Frame& p = tx.back();
  uint8_t exp1[8] = {0x21, 18, 0xFF, 0xFF, 1, 0x00, 1, 0xFF};
  assert(p.id == 0x1CE6FF90 && memcmp(p.d, exp1, 8) == 0);
  // tartás: 200 ms-onként ss=2
  s.update(2199); assert(tx.size() == n0 + 1);
  s.update(2200); assert(tx.back().d[6] == 2 && tx.back().d[1] == 18 && tx.back().d[4] == 1);
  s.update(2400); assert(lastStatus().d[6] == 2);
  // életjel 1 mp-enként (közben)
  int ka = 0, holds = 0;
  for (uint32_t t = 2401; t <= 3500; t++) {
    size_t before = tx.size();
    s.update(t);
    for (size_t i = before; i < tx.size(); i++) {
      if (tx[i].id == 0x1CE7FF90) ka++;
      if (tx[i].id == 0x1CE6FF90 && tx[i].d[6] == 2) holds++;
    }
  }
  assert(ka == 1 && holds == 5);
  // elengedés: ss=0, egyszer
  s.release(3501);
  assert(tx.back().d[6] == 0 && tx.back().d[1] == 18);
  size_t n1 = tx.size();
  s.release(3502); assert(tx.size() == n1);
  s.update(3800); // nincs több ss=2
  for (size_t i = n1; i < tx.size(); i++) assert(tx[i].id != 0x1CE6FF90);
  // újabb lenyomás: cc nő
  s.press(18, 4000); assert(tx.back().d[4] == 2);
  // másik gomb nyomása előbb elengedi az előzőt
  size_t n2 = tx.size();
  s.press(19, 4100);
  assert(tx.size() == n2 + 2);
  assert(tx[n2].d[1] == 18 && tx[n2].d[6] == 0);
  assert(tx[n2 + 1].d[1] == 19 && tx[n2 + 1].d[6] == 1);
  // stop: elenged
  s.stop(4200);
  assert(tx.back().d[1] == 19 && tx.back().d[6] == 0);
  assert(s.state() == MullerAuxOSender::State::Off);
  assert(!s.press(19, 4300));
}

static void test_sender_request_and_conflict() {
  tx.clear();
  MullerAuxOSender s; s.send = capture;
  s.begin(0x1ABCDE, 0x90, 0);
  s.update(300); assert(s.claimed());
  // Request for Address Claim (global)
  size_t n = tx.size();
  uint8_t req[3] = {0x00, 0xEE, 0x00};
  s.handleFrame(0x18EAFF26, req, 3, 400);
  assert(tx.size() == n + 1 && tx.back().id == 0x18EEFF90);
  // más címre szóló request: nem válaszol
  s.handleFrame(0x18EA2526, req, 3, 410);
  assert(tx.size() == n + 1);
  // saját visszhang: semmi
  uint8_t own[8]; uint64_t nm = s.name(); for (int i = 0; i < 8; i++) own[i] = nm >> (8 * i);
  s.handleFrame(0x18EEFF90, own, 8, 420);
  assert(tx.size() == n + 1 && s.claimed());
  // ütközés alacsonyabb prioritású (nagyobb NAME) eszközzel: mi maradunk, újra bejelentünk
  uint8_t hi[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  s.handleFrame(0x18EEFF90, hi, 8, 430);
  assert(s.claimed() && s.address() == 0x90 && tx.back().id == 0x18EEFF90);
  // ütközés magasabb prioritásúval (kisebb NAME): tartott gomb megszűnik, új cím
  s.press(20, 500);
  uint8_t lo[8] = {0x01, 0, 0, 0, 0, 0, 0, 0x00};
  s.handleFrame(0x18EEFF90, lo, 8, 510);
  assert(s.activeInput() == MullerAuxOSender::NO_INPUT);
  assert(s.state() == MullerAuxOSender::State::Claiming && s.address() == 0x91);
  assert(tx.back().id == 0x18EEFF91);
  assert(!s.press(20, 520));              // új cím érvényesítéséig nem küld
  s.update(760); assert(s.claimed());
  assert(s.press(20, 770) && tx.back().id == 0x1CE6FF91);
}

static void test_light() {
  // IO34 0dB-n van az érzékelő (sötétben magas, világosban alacsony), IO39 zajos, IO34 11dB kis változás
  uint16_t dark[4]   = {900, 3900, 1200, 4095};
  uint16_t bright[4] = {850, 1500, 1250, 4095};
  assert(lightPickChannel(dark, bright) == 1);
  // telített csatorna kimarad, kis különbség nem érzékelő
  uint16_t d2[4] = {4095, 4095, 1000, 1000}, b2[4] = {4095, 4095, 1050, 1080};
  assert(lightPickChannel(d2, b2) == LIGHT_NONE);
  // fordított irány is működik
  uint16_t d3[4] = {100, 0, 0, 0}, b3[4] = {2100, 0, 0, 0};
  assert(lightPickChannel(d3, b3) == 0);
  // szint: iránytól függetlenül 0 = sötét, 1000 = világos, vágás
  assert(lightLevel(3900, 3900, 1500) == 0 && lightLevel(1500, 3900, 1500) == 1000);
  assert(lightLevel(2700, 3900, 1500) == 500);
  assert(lightLevel(4095, 3900, 1500) == 0 && lightLevel(100, 3900, 1500) == 1000);
  assert(lightLevel(100, 100, 2100) == 0 && lightLevel(2100, 100, 2100) == 1000);
  assert(lightLevel(5, 7, 7) == 500);
  // fényerő
  assert(lightBrightness(0, 20, 255) == 20 && lightBrightness(1000, 20, 255) == 255);
  assert(lightBrightness(500, 20, 255) == 138);
  // simítás: lassan követ, reset azonnal
  uint32_t st = 0;
  assert(lightSmooth(st, 100, true) == 100);
  uint16_t v = 0;
  for (int i = 0; i < 3; i++) v = lightSmooth(st, 200, false);
  assert(v > 100 && v < 200);
  for (int i = 0; i < 80; i++) v = lightSmooth(st, 200, false);
  assert(v >= 198 && v <= 200);
}

static void test_layout() {
  uint8_t sl[3][8], nm[3];
  layoutDefault(sl, nm);
  // gyári kiosztás: TRAIL B3 üres, B5 = 3 (középre), keret B2 üres, permetezés B4 = 12
  assert(sl[0][2] == SLOT_EMPTY && sl[0][4] == 3 && sl[2][1] == SLOT_EMPTY && sl[1][3] == 12);
  assert(nm[0] == 0 && nm[1] == 1 && nm[2] == 2);
  assert(strcmp(tabName(1, true), "SPRAY") == 0 && strcmp(tabName(6, false), "FŐ") == 0);
  // kevert kiosztás: az 1. fülre keret emelés (18) és permetezés adag+ (15)
  sl[0][0] = 18; sl[0][1] = 15;
  assert(slotInput(sl[0][0]) == 18 && slotIcon(sl[0][0]) == MIcon::BoomUp);
  assert(strcmp(slotLabel(sl[0][1], true), "Rate +") == 0);
  // alternatív ikon
  uint8_t c = 4 | SLOT_ALT;
  assert(slotInput(c) == 4 && slotIcon(c) == MIcon::AxleIn && slotAlt(c));
  assert(slotIcon(5) == MIcon::FoldOut);
  // üres / kiosztatlan
  assert(!slotActive(SLOT_EMPTY) && !slotActive(17) && slotIcon(17) == MIcon::None);
  // fül láthatóság
  for (int b = 0; b < 8; b++) sl[0][b] = SLOT_EMPTY;
  assert(!layoutTabVisible(sl, 0) && layoutFirstVisible(sl) == 1);
  assert(layoutNextVisible(sl, 1) == 2 && layoutNextVisible(sl, 2) == 1);
  // választó elemek
  uint8_t it[12];
  assert(pickerItems(0, it) == 10 && it[8] == (4 | SLOT_ALT) && it[9] == (5 | SLOT_ALT));
  assert(pickerItems(1, it) == 8 && it[0] == 8 && it[2] == 10 && it[3] == 12);   // B1, B2, B3, B4...
  assert(pickerItems(2, it) == 7);                                              // keret B2 kiosztatlan
  // régi beállítás átvétele
  uint8_t cfg[24];
  for (int i = 0; i < 24; i++) cfg[i] = 1;
  cfg[2] = 0; cfg[4] = 2;
  layoutFromLegacy(cfg, sl);
  assert(sl[0][2] == SLOT_EMPTY && sl[0][3] == (4 | SLOT_ALT) && sl[0][4] == 3 && sl[2][1] == SLOT_EMPTY);
}

int main() {
  test_layouts();
  test_touchfilter();
  test_name();
  test_funcs();
  test_light();
  test_layout();
  test_sender_basic();
  test_sender_request_and_conflict();
  puts("MINDEN TESZT OK");
  return 0;
}
