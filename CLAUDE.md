# CLAUDE.md – AUX-N ISOBUS Touch Joystick

## A projekt
Érintőkijelzős ISOBUS AUX-N bemeneti eszköz (Auxiliary Input Type 2) ESP32-re. 3/6/8 gombos elrendezés, fölé cserélhető 3D nyomtatott keret (tapintható vezető). Terminál: Müller (gyári), tesztre AgISOVirtualTerminal is.

## Kommunikáció
- A felhasználóval **magyarul** beszélj. A kijelzőn megjelenő szövegek is magyarok, ékezettel.
- A felhasználó végzi a valós hardveres tesztet. A firmware-t a GitHub Actions fordítja (.bin artifact), Espressif online flasherrel égeti.
- A „Készítette: Balogh Dávid & Claude” felirat maradjon a nyitóképen és a beállítások oldalon.

## Hardver
- Most: 2.8" ESP32-32E (E32R28T), lásd docs/HARDVER.md. CAN: SN65HVD230, TX=IO27, RX=IO35.
- Később: Waveshare ESP32-S3-Touch-LCD-4 (lásd docs/TERV.md).

## Felépítés
- `src/config.h`: lábak, verzió, érintés paraméterek
- `src/lgfx_board.h`: LovyanGFX config, panel futásidőben választható (init előtt)
- `src/keys.*`: elrendezés + érintésszűrő, hardverfüggetlen (natívan tesztelhető)
- `src/can_monitor.*`: TWAI figyelő (1. fázis: nem küld semmit)
- `src/feedback.*`: LED + hang
- `src/settings.*`: NVS (Preferences)
- `src/fonts_hu.h`: GENERÁLT (`python3 tools/make_vlw.py`), ne szerkeszd kézzel

## Szabályok
- Az AUX-N object poolban mindig 8 bemenet legyen, állandó ID-kkal (a hozzárendelések megmaradása miatt).
- Biztonság: gomb soha ne maradhasson „beragadva”. Érintés-ugrásnál, képernyőváltásnál és hibánál minden gomb elengedett állapotba kerül.
- Verzió: `FW_VERSION` a config.h-ban, minden kiadott változtatásnál emeld.
