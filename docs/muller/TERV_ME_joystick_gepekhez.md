# Terv: ME (Müller) joystick támogatás a permetező, szóró és vetőgép firmware-ben

Készítette: Balogh Dávid & Claude, 2026-11-25
Repók: `Isobus_Esp_Sprayer`, `Isobus_Spreader_esp32`, `Isobus_ESP_seeder_V2`
Ez a fájl mindhárom repóba ugyanígy kerüljön be: `docs/TERV_ME_joystick.md`, a CLAUDE.md-ből hivatkozva.

## 1. Összefoglaló

A gyári Müller (ME) permetező joystick szabványos ISOBUS AUX-O üzeneteket küld. Bemértük, és a gyári
Müller Jobcomputerrel visszaellenőriztük (Python emulátor CANable-en keresztül). A saját gépvezérlők
ugyanígy hallgathatják:

- **Permetező:** a gyári Müller kiosztás szerint működik (fix funkciósor), a szabad gombok állíthatók.
- **Szóró és vetőgép:** a menüben gombonként beállítható, melyik joystick gomb mit csinál. Kevesebb funkció kell,
  és nem a Müller kiosztás szerint.
- Mindhárom gépen a beállításokban választható a joystick forrás: **AUX-N / ME joystick / mindkettő**.
- A home made AgIsoAux joystick a tervezett Müller módban ugyanígy jelentkezik be (lásd `TERV_muller_auxo.md`
  az AgIsoAuxInputs forkban), ezért ott külön munka nincs: ha az a Müller NAME-mel küld, ezek a gépek is értik.

## 2. A protokoll (vevő oldal)

| Mező | Érték |
|---|---|
| CAN ID | `0x1CE6FFxx`: prio 7, PGN 0xE600 (VT->ECU), cél global, xx = joystick címe (mérve 0xCF, de változhat) |
| Adat | `21 nn FF FF cc 00 ss FF` |
| `21` | AUX-O (Auxiliary Input Type 1) állapot |
| `nn` | gombszám 0..23 = váltóállás × 8 + belső index |
| `cc` | számláló-szerű, nem használjuk |
| `ss` | 0 = elengedve, 1 = lenyomva (egyszer), 2 = tartva (~200 ms-onként) |
| NAME | gyártó 98, function 128, device class 13 (az identity eszközönként más) |

A joystick felismerése a NAME alapján történik, nem a cím alapján. Az üzenet globális, nem a mi címünkre jön,
ezért a fogadáshoz olyan AgIsoStack callback kell, ami bármely küldő 0xE600-as üzenetét megkapja, vagy a nyers
TWAI keretet kell a modulnak átadni. A pontos API-t a repóban használt AgIsoStack verzióban kell ellenőrizni.
Figyelni kell, hogy a saját VT-kliensünk is kap 0xE600-at a terminálunktól: a modul csak a `21`-es kódú
üzeneteket veszi, és csak a Müller NAME-ű küldőtől.

### Gombszámok

A váltó állása: felső = 0 (TRAIL), középső = 1 (permetezés), alsó = 2 (keret). A joystick oszloponként
számoz, ezért a B4 és a B5 fel van cserélve.

```
Gombok helye:   B1 | B3 B4
                B2 | B5 B6
                   | B7 B8
```

| Gomb | Felső | Középső | Alsó |
|---|---|---|---|
| B1 | 0 | 8 | 16 |
| B2 | 1 | 9 | 17 |
| B3 | 2 | 10 | 18 |
| B4 | 4 | 12 | 20 |
| B5 | 3 | 11 | 19 |
| B6 | 5 | 13 | 21 |
| B7 | 6 | 14 | 22 |
| B8 | 7 | 15 | 23 |

## 3. Közös modul: `MullerJoystickAuxO.h` (v2)

- Egyetlen header, mindhárom repóban **azonos** (`MULLER_JOY_VERSION 2`). Ha változik, mindhárom helyen frissíteni kell.
- Csak dekódol: `onEvent(input 0..23, Pressed/Held/Released)`. Hogy a gomb mit csinál, az a gép akciótáblája.
- Segédfüggvények: `layerOf()`, `buttonOf()` (B1..B8), `inputOf(layer, button)`, `layerName()`,
  `mullerDefaultName()` (a gyári Müller jelentés kijelzéshez).
- Biztonság: ha tartott gombnál 600 ms-ig nem jön "tartva", vagy a joystick eltűnik vagy más veszi át a címét,
  akkor Released eseményt küld (`releaseAll()`).
- `enabled = false` esetén semmilyen eseményt nem ad: ezt a gép beállítása kapcsolja.
- A valódi CAN naplón tesztelve: mind a 24 gomb a jó számra ad eseményt.

## 4. Közös gépoldali szerkezet (mindhárom gépen így)

### 4.1 Akciók

Minden gép felsorolja a saját akcióit (enum + rövid kijelzett név). Az akció fajtája határozza meg,
hogyan reagál az eseményekre:

| Fajta | Pressed | Held | Released | Példa |
|---|---|---|---|---|
| Kapcsoló (toggle) | vált | - | - | fő be/ki, munkalámpa |
| Lépés (step) | egyet lép | 0,6 mp tartás után 0,3 mp-enként ismétel | - | adag +/-, szakasz be/ki |
| Pillanatnyi (momentary) | kimenet BE | (életben tartja) | kimenet KI | keret emelés, hajlítás |
| Egyszeri (oneshot) | végrehajt | - | - | célérték 100% |

A pillanatnyi akció **csak tartás közben** aktív, soha nem reteszel. A modul 600 ms-os időtúllépése miatt
busz hiba esetén magától leáll.

### 4.2 Kiosztás tárolása (NVS)

- `me_source`: 0 = csak AUX-N, 1 = csak ME joystick, 2 = mindkettő
- `me_map[24]`: gombszámonként egy akció (vagy 0 = nincs). A permetezőnél gyári értékekkel indul (lásd 5.),
  a szórónál és a vetőgépnél a 6. és 7. pontban megadott alapkiosztással.
- "Alaphelyzet" funkció: visszaállítja a gép alapkiosztását.

### 4.3 Menü ("ME joystick" oldal)

- **Forrás választó:** AUX-N / ME joystick / mindkettő.
- **Állapotsor:** "ME joystick: látható (0xCF) / nincs a buszon", és az utoljára nyomott gomb
  (pl. "Középső B2"). Ez a legegyszerűbb hibakeresés a terepen.
- **Kiosztás:** 3 lap a váltó állásai szerint (Felső / Középső / Alsó), lapon a 8 gomb a joystick elrendezése
  szerint rajzolva (B1 | B3 B4 / B2 | B5 B6 / B7 B8). Minden gombnál lenyíló lista a gép akcióival + "nincs".
- **Tanuló mód (ajánlott):** "Nyomd meg a joystick gombot" → a menü kijelöli azt a gombot, és ott választod az akciót.
  Így nem kell tudni, melyik gomb melyik szám.
- Ugyanez a beállítás a szervizmódú weboldalon is (a webszerver alapból ki van kapcsolva, csak igény szerint indul).
- A VT stílus a gép meglévő menüje szerint (szürke háttér, softkey-ek, mint eddig).

### 4.4 Együttélés az AUX-N-nel

- Az AUX-N kiosztást a VT intézi, az ME kiosztás a gépen belül van. A kettő ugyanarra az akciótáblára hívhat.
- "Mindkettő" módban, ha ugyanaz az akció mindkét joystickon ki van osztva, mindkettő működik. Ez szándékos.

## 5. Permetező (`Isobus_Esp_Sprayer`)

Alapból a gyári Müller kiosztás. A középső és az alsó állás funkciói fixek, mert ez a joystick erre készült.
A felső (TRAIL) állás és a szabad gombok viszont állíthatók.

| Gombszám | Müller funkció | Saját permetező akció | Fajta |
|---|---|---|---|
| 8 | Célérték 100% | adag visszaállítása a célértékre | egyszeri |
| 9 | Szakasz-főkapcsoló | fő szakaszkapcsoló be/ki | kapcsoló |
| 10 | Szakaszok be jobbról balra | a következő szakasz bekapcsolása jobbról | lépés |
| 12 | Szakaszok be balról jobbra | a következő szakasz bekapcsolása balról | lépés |
| 11 | Szakaszok ki balról jobbra | a következő szakasz kikapcsolása balról | lépés |
| 13 | Szakaszok ki jobbról balra | a következő szakasz kikapcsolása jobbról | lépés |
| 14 | Adag − | auto: célérték −10%, kézi: szelep zár | lépés |
| 15 | Adag + | auto: célérték +10%, kézi: szelep nyit | lépés |
| 16 | Keret kézi/auto | (később, keretautomatika) | kapcsoló |
| 18 / 19 | Keret emelés / süllyesztés | hidraulika kimenet (ha be van állítva) | pillanatnyi |
| 20 / 21 | Behajlítás / kihajlítás | hidraulika kimenet | pillanatnyi |
| 22 / 23 | Bal fel jobb le / Jobb fel bal le | hidraulika kimenet | pillanatnyi |
| 0–7, 17 | TRAIL és szabad gombok | választható: munkalámpa, keverő, szűrőmosás, ventilátor stb., vagy nincs | fajta szerint |

- A szakaszléptetés a 12 szakaszra (quattro szórófejtartó) vonatkozik. A szakaszok sorrendje balról jobbra
  1..12, a "jobbról balra bekapcsolás" a legjobboldalibb kikapcsolt szakasszal kezdi.
- A keret hidraulika akciói csak akkor aktívak, ha a gépen a hidraulika kimenetek be vannak állítva.
  Egyébként "nincs kiosztva" jelzés jön a VT-n.
- A ±10% lépés a gyári Müller viselkedése. Ha a gépen más lépésköz van beállítva, azt kell használni.

## 6. Szóró (`Isobus_Spreader_esp32`)

Akciók (a tervezett AUX-N funkciókkal egyezően):

| Akció | Fajta |
|---|---|
| Szórás be/ki | kapcsoló |
| Adag + % (kézi módban: szalag gyorsítás) | lépés |
| Adag − % (kézi módban: szalag lassítás) | lépés |
| Adag 100% (célértékre vissza) | egyszeri |
| Kézi / auto váltás | kapcsoló |
| Határszórás be/ki | kapcsoló |
| Munkalámpa 1 be/ki | kapcsoló |
| Munkalámpa 2 be/ki | kapcsoló |

A súber (kapu) nem joystick funkció: kézzel állítják a gépen, és csak a menüben adják meg.

Alapkiosztás (a középső álláson, hogy a kéz ugyanott maradjon, mint permetezésnél; menüben átírható):

| Gomb | Gombszám | Akció |
|---|---|---|
| B1 | 8 | Adag 100% |
| B2 | 9 | Szórás be/ki |
| B3 | 10 | Határszórás be/ki |
| B4 | 12 | Kézi / auto |
| B5 | 11 | Munkalámpa 1 |
| B6 | 13 | Munkalámpa 2 |
| B7 | 14 | Adag − |
| B8 | 15 | Adag + |
| többi | – | nincs |

A szóró az Isobus_Spreader_esp32 két célboardján (Waveshare ESP32-S3-ETH-8DI-8RO-C és RC_ESP32) is fut: mindkettőn ugyanaz a modul.

## 7. Vetőgép v2 (`Isobus_ESP_seeder_V2`)

Akciók (a meglévő v2 funkciók alapján; ami a gépprofilban nincs bekapcsolva, az a listában ne jelenjen meg):

| Akció | Fajta |
|---|---|
| Vetés be/ki (fő) | kapcsoló |
| 1. adagoló be/ki | kapcsoló |
| 2. adagoló be/ki (2 tartályos gépen) | kapcsoló |
| Adag + % | lépés |
| Adag − % | lépés |
| Adag 100% | egyszeri |
| Kézi / auto (AgOpenGPS szakaszvezérlés) | kapcsoló |
| Munkalámpa be/ki | kapcsoló |

A leforgatás (crank test) szándékosan **nincs** a joystick akciók között: álló gépen, a menüből indítandó,
egy véletlen gombnyomás ne indítsa el.

Alapkiosztás (középső állás):

| Gomb | Gombszám | Akció |
|---|---|---|
| B1 | 8 | Adag 100% |
| B2 | 9 | Vetés be/ki |
| B3 | 10 | 1. adagoló be/ki |
| B4 | 12 | 2. adagoló be/ki |
| B5 | 11 | Kézi / auto |
| B6 | 13 | Munkalámpa |
| B7 | 14 | Adag − |
| B8 | 15 | Adag + |
| többi | – | nincs |

A vetőgép v2 mindkét boardon (Waveshare ESP32-S3-ETH-8DI-8RO-C fő board és RC1) párhuzamosan fejlesztendő.
A modul boardfüggetlen, csak a CAN fogadás bekötése más.

## 8. Biztonsági szabályok (mindhárom gépen)

1. Hidraulikát mozgató akció csak **pillanatnyi** lehet: tartás közben aktív, elengedésre, időtúllépésre és
   joystick-vesztésre azonnal leáll.
2. ME forrás kikapcsolva (`me_source` = AUX-N) → `joy.enabled = false`, semmilyen ME esemény nem hat.
3. Joystick-vesztés (Address Claim más NAME-mel ugyanazon a címen, vagy hosszú csend) → `releaseAll()`.
4. A lépés akciók ismétlése a géprészen történik (Held + saját időzítő), a modul nem ismétel.
5. Leforgatás, kalibrálás, önhangolás és szervizfunkció soha nem rendelhető joystick gombhoz.

## 9. Megvalósítási sorrend

1. `MullerJoystickAuxO.h` v2 bemásolása mindhárom repóba, a CAN fogadás rákötése, és egy soros/debug kiírás
   eseményenként (még akció nélkül).
2. **Permetező**: fix kiosztás, akciótábla, menü (forrás + szabad gombok), teszt.
3. **Szóró**: akciótábla, állítható kiosztás, menü (3 lap + tanuló mód), mindkét boardon.
4. **Vetőgép v2**: ugyanaz, mindkét boardon. A gépprofil átalakítással együtt is mehet, ha az épp napirenden van.

## 10. Tesztelés

- **Emulátor:** `joystick_emulator.py` a CANable-en (`seq` = mind a 24 gomb sorban, `k 2` = középső B2 stb.).
  A gép debug kiírásán vagy a VT-n figyelni, hogy a jó akció fut-e.
- **Biztonsági teszt:** `a 3 5` (alsó B3, 5 mp tartás), közben a CANable-t kihúzni → 0,6 mp-en belül le kell állnia.
- **Valódi joystick:** a `joystick_bemero.py --aux --sa 0xCF` élőben mutatja, mit küld.
- **Nyitott kérdés, ezt először ellenőrizni kell:** a valódi ME joystick küld-e állapotüzenetet akkor is, ha
  **nincs Müller Jobcomputer a buszon**, csak a terminál és a saját gépvezérlő. A bemérés a Jobcomputerrel együtt
  történt. Ha nélküle nem küld, a gépvezérlőnek a Jobcomputer felé menő választ kell utánoznia: ezt a nyers
  naplóból (`muller_joystick_map_raw.csv`) kell kiolvasni.

## Kapcsolódó fájlok

- `MullerJoystickAuxO.h` (v2): közös vevő modul
- `joystick_emulator.py`: ME joystick emulátor CANable-hez (referencia NAME, keretek, időzítés)
- `joystick_bemero.py`: bemérő és élő AUX kiíró
- `TERV_muller_auxo.md`: a home made joystick Müller módja (AgIsoAuxInputs fork)
