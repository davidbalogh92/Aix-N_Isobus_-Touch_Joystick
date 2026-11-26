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
#include "muller_profile.h"
#include "light_logic.h"

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

static void test_profiles() {
  uint8_t c[24];
  // Vontatott (saját): B3 rejtve, B4/B6 keret ikon, minden más be
  mullerApplyProfile(PROF_TOWED_OWN, c);
  assert(!mullerBtnActive(c, 2) && mullerBtnActive(c, 3) && mullerBtnActive(c, 4));
  assert(mullerIconFor(c, 4) == MIcon::FoldIn && mullerIconFor(c, 5) == MIcon::FoldOut);
  assert(!mullerBtnActive(c, 17));
  assert(mullerProfileOf(c) == PROF_TOWED_OWN);
  assert(mullerLayerVisible(c, 0) && mullerLayerVisible(c, 1) && mullerLayerVisible(c, 2));
  // JB3 gyári: lakat be, B4/B6 tandem ikon
  mullerApplyProfile(PROF_TOWED_JB3, c);
  assert(mullerBtnActive(c, 2) && mullerIconFor(c, 4) == MIcon::AxleIn && mullerIconFor(c, 5) == MIcon::AxleOut);
  assert(mullerProfileOf(c) == PROF_TOWED_JB3);
  assert(strcmp(mullerLabelFor(c, 4, true), "Axle \xE2\x86\x92\xE2\x86\x90") == 0);
  // Önjáró: TRAIL fül eltűnik, BOOT a maradék két fül közt vált
  mullerApplyProfile(PROF_SELF, c);
  for (int i = 0; i < 8; i++) assert(!mullerBtnActive(c, i));
  assert(!mullerLayerVisible(c, 0) && mullerFirstVisible(c) == 1);
  assert(mullerNextVisible(c, 1) == 2 && mullerNextVisible(c, 2) == 1 && mullerNextVisible(c, 0) == 1);
  assert(mullerProfileOf(c) == PROF_SELF);
  // szerkesztés: Be -> Alt -> Ki -> Be (B4), Be -> Ki -> Be (B5), kiosztatlan mindig Ki
  mullerApplyProfile(PROF_TOWED_OWN, c);
  assert(mullerNextState(c, 4) == BTN_ALT); c[4] = BTN_ALT;
  assert(mullerNextState(c, 4) == BTN_OFF); c[4] = BTN_OFF;
  assert(mullerNextState(c, 4) == BTN_ON);
  assert(mullerNextState(c, 3) == BTN_OFF);
  assert(mullerNextState(c, 17) == BTN_OFF);
  assert(mullerProfileOf(c) == PROF_CUSTOM);   // c[4] = OFF -> egyéni
  // minden ki: nincs látható fül
  for (int i = 0; i < 24; i++) c[i] = BTN_OFF;
  assert(mullerFirstVisible(c) == 0xFF && mullerNextVisible(c, 1) == 0xFF);
  // csak egy fül: önmagára lép
  c[9] = BTN_ON;
  assert(mullerNextVisible(c, 1) == 1 && mullerFirstVisible(c) == 1);
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

int main() {
  test_layouts();
  test_touchfilter();
  test_name();
  test_funcs();
  test_profiles();
  test_light();
  test_sender_basic();
  test_sender_request_and_conflict();
  puts("MINDEN TESZT OK");
  return 0;
}
