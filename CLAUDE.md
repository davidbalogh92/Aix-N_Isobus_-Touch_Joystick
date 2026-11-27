# CLAUDE.md – AUX-N ISOBUS Touch Joystick

## A projekt
Érintőkijelzős ISOBUS AUX-N bemeneti eszköz (Auxiliary Input Type 2) ESP32-re. 3/6/8 gombos elrendezés, fölé cserélhető 3D nyomtatott keret (tapintható vezető). Terminál: Müller (gyári), tesztre AgISOVirtualTerminal is.

## Kommunikáció
- A felhasználóval **magyarul** beszélj. A kijelzőn megjelenő szövegek is magyarok, ékezettel.
- A felhasználó végzi a valós hardveres tesztet. A firmware-t a GitHub Actions fordítja (.bin artifact), Espressif online flasherrel égeti.
- A „Készítette: Balogh Dávid & Claude” felirat maradjon a nyitóképen és a beállítások oldalon.

## Hardver
- Fő cél: 3.5" ESP32-32E (E32R35T, ST7796, touch közös SPI), CAN: SN65HVD230, TX=IO25, RX=IO35.
- Másodlagos: 2.8" ESP32-32E (E32R28T), CAN TX=IO27, RX=IO35. Lapválasztás: platformio env (BOARD_E32R35T / BOARD_E32R28T). Részletek: docs/HARDVER.md.
- Később: Waveshare ESP32-S3-Touch-LCD-4 (lásd docs/TERV.md).

## Felépítés
- `src/config.h`: lábak, verzió, érintés paraméterek
- `src/lgfx_board.h`: LovyanGFX config, panel futásidőben választható (init előtt)
- `src/keys.*`: elrendezés + érintésszűrő, hardverfüggetlen (natívan tesztelhető)
- `src/can_monitor.*`: TWAI figyelő + canSend + RX hook (magától nem küld; csak a Müller mód)
- `src/feedback.*`: LED + hang
- `src/settings.*`: NVS (Preferences)
- `src/muller_out.*`: Müller AUX-O küldő (address claim, ss=1/2/0, cc, 0xE700), hardverfüggetlen
- `src/muller_funcs.h`: Müller funkciótábla (ikon, felirat), `src/muller_icons.h`: ikonok (sablon, PC-n is renderelhető)
- `test/native_test.cpp`: natív tesztek (CI futtatja). Ikon/képernyő változás után frissítsd a `docs/muller_elonezet.png`-t (tools/icon_preview.*)
- `src/light_logic.h`: fényérzékelő (R21 LDR) csatornaválasztás, szint, simítás (natívan tesztelt). A láb nem dokumentált: betanításkor IO34/IO39 × 11dB/0dB közül választ
- `src/ota.*`: WiFi OTA (AP `AUXN-Joy-XXXX` / `auxnjoy1`, feltöltő oldal, csak 0xE9 kezdetű app kép), csak a Frissítés képernyőn él; WiFi alapból KI
- `src/splash_art.h`: nyitókép grafika (sablon), előnézet: docs/nyitokep_elonezet.png
- Rezgőmotor: félretéve a Waveshare-es változathoz (a felhasználó döntése)
- `src/fonts_hu.h`: GENERÁLT (`python3 tools/make_vlw.py`), ne szerkeszd kézzel

## Müller mód
- Terv: docs/TERV_muller_mod.md. Referencia (bemért, mérvadó): docs/muller/ (TERV_muller_auxo.md, MullerJoystickAuxO.h v2).
- A felhasználó gépe: TRAIL B5 (gombszám 3) = vonórúd középre (tartásos); TRAIL B4 (4) = keret összecsukás, B6 (5) = keret kinyitás (tartásos; hivatalosan tandem-híd funkció); TRAIL B3 (2) = gyári lakat, nála nincs.
- Szabad kiosztás (v0.7.0): 3 fül × 8 hely, hely kód = gombszám (+0x80 alt ikon, 0xFF üres), fülnevek listából: `src/muller_layout.h` (natívan tesztelt). Alap = gyári Müller kiosztás, TRAIL B3 üres. NINCS profilválasztó (a felhasználó kérése). Szerkesztő → funkcióválasztó képernyő. Téma: JB3 (sötét gombok, zöld nyilak, kék szórófej), fülek: TRAIL piros, SPRAY sárga, BOOM zöld.
- Nyelv: a kijelző EN/HU (`tr(en, hu)` a main.cpp-ben), alapból angol; a soros log magyar. A kredit felirat mindig magyar.

## Szabályok
- Az AUX-N object poolban mindig 8 bemenet legyen, állandó ID-kkal (a hozzárendelések megmaradása miatt).
- Biztonság: gomb soha ne maradhasson „beragadva”. Érintés-ugrásnál, képernyőváltásnál és hibánál minden gomb elengedett állapotba kerül.
- Verzió: `FW_VERSION` a config.h-ban, minden kiadott változtatásnál emeld.
