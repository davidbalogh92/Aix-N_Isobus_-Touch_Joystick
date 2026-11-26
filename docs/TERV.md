# Fejlesztési terv

## 1. fázis: hardver ébresztés (v0.1–0.2) ← most

- [x] 3.5" E32R35T támogatás (fő cél), 2.8" E32R28T másodlagos
- [x] Kijelző, önteszt, futásidőben választható panel (ILI9341 / ST7789), invert, BGR
- [x] Érintés kalibrálás NVS-be
- [x] 3 / 6 / 8 gombos elrendezés, BOOT-tal váltható
- [x] Rezisztív érintés szűrés (stabil lenyomás, ugrás = elengedés)
- [x] Visszajelzés: LED, hang, soros log
- [x] Tájolás: fekvő / álló / 180° (v0.3.0), álló módban a gombrács is átrendeződik
- [x] CAN diagnosztika képernyő
- [x] CAN figyelés (TWAI, 250k, csak hallgat): forgalom, eszközszám, VT jelenlét
- [ ] Teszt a valós lapon: színek, érintés pontosság, CAN forgalom a Müller terminállal

## 2. fázis: Müller (ME) mód (v0.4.0: megépítve, busz teszt hátra)

Részletek: [TERV_muller_mod.md](TERV_muller_mod.md). Gyorsabb, mint az AUX-N (nincs object pool), és a gyári Jobcomputerrel azonnal tesztelhető.

## 2b. fázis: ISOBUS AUX-N bemeneti eszköz

- AgIsoStack++ (a meglévő AgIsoAuxInputs joystick alapján): address claim, VT kliens, object pool feltöltés
- Object pool: **mindig 8 db Auxiliary Input Type 2** objektum, állandó ID-kkal. Az elrendezés csak a megjelenítést változtatja, így a terminálon a hozzárendelések megmaradnak.
- Auxiliary Input Type 2 Status üzenetek küldése a `g_keyDown[]` alapján
- A gombok a terminálon is nevet kapnak (designator)
- AP módú webes debug oldal, mint a meglévő joysticknál (kikapcsolva alapból)

## 3. fázis: kényelmi funkciók

- Hozzárendelés-lehallgatás (Auxiliary Assignment Type 2 a busz forgalmából): a kijelző mutatja, melyik bemenetre van funkció kiosztva, és jelzi, ha rejtett (pl. 3 gombos módban a 7-es) bemenetnek van hozzárendelése
- Keretfelismerés: mágnes a keretben + 2 Hall-szenzor → automatikus 3/6/8 váltás
- Rezgőmotor (haptikus visszajelzés)

## 4. fázis: végleges hardver

- Waveshare ESP32-S3-Touch-LCD-4 (480×480, IPS, kapacitív multitouch, 7–36 V bemenet, TJA1051 CAN, sorkapocs)
- vagy marad a 3,5" ESP32-32E (E32R35T) + SN65HVD230 + 12 V tápegység
- Keret: ASA vagy PETG, 2–4 mm mély lyukak, árnyékoló perem
