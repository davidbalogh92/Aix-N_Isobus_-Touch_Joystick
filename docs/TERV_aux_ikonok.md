# Terv: AUX-N funkció ikonok a Touch Joystickon (saját gépek)

Készítette: Balogh Dávid & Claude, 2026-11-26
Állapot: terv. Előfeltétel: az AUX-N réteg (a joystick valódi AUX-N bemeneti eszközként jelentkezik be a terminálra).

## Cél

Ha a terminálon egy funkciót hozzárendelünk a joystick egyik gombjához, a joystick kijelzőjén is jelenjen meg a funkció
ikonja és neve. Első körben csak a **saját 3 gépnél**: vetőgép v2 (`Isobus_ESP_seeder_V2`), szóró
(`Isobus_Spreader_esp32`), permetező (`Isobus_Esp_Sprayer`). Mindhárom firmware-t mi írjuk, ezért a munkagép közvetlenül
elküldheti az adatot.

Idegen gépek (a munkagép object pooljának lehallgatása) **később**, ha lesz kísérleti eszköz. Lásd a végén.

## Miért nem elég a terminál

- A hozzárendelést a terminál küldi a munkagépnek (Auxiliary Assignment Type 2). Ez 14 bájtos, TP-n megy, és a funkció
  csak objektum-azonosítóval szerepel benne.
- A funkció képe és neve a munkagép object pooljában van, ami csak feltöltéskor megy át a buszon. A Müller terminál
  eltárolja a poolt, ezért a legtöbb bekapcsoláskor a kép nem megy át. A terminált nem lehet megkérni egy másik eszköz
  poolja tartalmára.

## Megoldás: a munkagép mondja meg

A munkagép (function owner) tudja, melyik funkciója melyik bemeneti eszköz melyik bemenetére van hozzárendelve, mert a
hozzárendelést ő kapja. Ezt egy saját üzenetben körbeküldi:

> „A 0x__ című bemeneti eszköz 3-as bemenetére a(z) *Adag +* funkcióm került.”

A joystick csak a saját címére szóló sorokat veszi, és a közös katalógusból kirakja az ikont és a nevet (EN/HU).
TP-lehallgatás és poolfeldolgozás nem kell.

### Üzenet (javaslat)

- **PGN 0xFFA5** (Proprietary B, broadcast, prioritás 6). Nem ütközik a külső kijelző Proprietary A protokolljával
  (`Isobus_ExtDisplay_esp32`). *Ellenőrizni kell, hogy egyik projekt sem használja már a 0xFFA5-öt.*
- Adat (8 bájt):

| Bájt | Tartalom |
|---|---|
| 0 | üzenettípus: `0x01` = hozzárendelés-címke, `0x02` = „nincs hozzárendelés” (törlés) |
| 1 | a bemeneti eszköz (joystick) címe |
| 2–3 | a bemenet objektum-azonosítója (little endian) — a joystick poolja szerint |
| 4–5 | katalógus-azonosító (little endian), lásd lent |
| 6 | jelzők: bit0 = tartásos (mozgató) funkció, bit1 = kapcsoló (be/ki állapot követhető) |
| 7 | állapot: 0xFF = nincs; kapcsoló funkciónál 0/1 = ki/be (később a gombon jelezhető) |

- Mikor megy ki:
  - minden hozzárendelés-változáskor (Assignment parancs után) az érintett sorok,
  - a joystick Request-jére (PGN 0xEA00, kért PGN 0xFFA5) az összes aktuális sor,
  - és lassan, ciklikusan (pl. 5 mp-enként egy kör), hogy a később bekapcsolt joystick is megkapja.

### Közös katalógus: `AuxFunctionCatalog.h`

Egyetlen fejléc, a joystickban és a három gépvezérlőben **azonos** (verziószámmal, mint a `MullerJoystickAuxO.h`).
Katalógus-azonosító → ikon + rövid név (EN/HU) + fajta. Az ikonokat a joystick rajzolja (a meglévő JB3 stílusú
`muller_icons.h` bővítése), a gép csak a számot küldi.

Első kör (a meglévő akciótáblák alapján, `docs/muller/TERV_ME_joystick_gepekhez.md`):

| Csoport | Funkciók |
|---|---|
| Közös | Adag + %, Adag − %, Adag 100%, Kézi/auto, Munkalámpa 1, Munkalámpa 2 |
| Szóró | Szórás be/ki, Határszórás be/ki |
| Vetőgép | Vetés be/ki, 1. adagoló be/ki, 2. adagoló be/ki |
| Permetező | Főkapcsoló, Szakasz be/ki jobbról/balról (4), Keret emelés/süllyesztés, Behajlítás/kihajlítás, Bal fel/Jobb fel, Keverő, Szűrőmosás, Ventilátor |

A számozás csoportonként tartalékkal (pl. közös 1–49, szóró 50–99, vetőgép 100–149, permetező 150–199), hogy bővíthető
legyen.

## Joystick oldal

- AUX-N módban a gombokon a hozzárendelt funkció ikonja és neve; ha nincs hozzárendelés: a gomb száma, halványan.
- Kapcsoló funkciónál (ha a gép küld állapotot) a gomb jelezheti a be/ki állapotot (pl. zöld csík).
- Ha egy rejtett bemenetre (pl. 3 gombos módban a 7-esre) érkezik hozzárendelés, figyelmeztetés a státuszsorban.
- A tanult adat nem kell, hogy megmaradjon: minden bekapcsoláskor a géptől jön (Request).

## Gép oldal (mindhárom repó)

- Az AgIsoStack hozzárendelés-kezelésénél (a pontos API-t a repóban használt verzióban ellenőrizni kell) a
  funkció objektum-azonosítója mellé a katalógus-azonosító.
- A címke üzenet küldése a fenti szabályok szerint. A bemeneti eszköz címét a hozzárendelésben kapott NAME-ből
  kell kikeresni (address claim tábla).

## Megvalósítási sorrend

1. CAN-teszt a joystickkal (Müller mód a gyári Jobcomputerrel).
2. AUX-N réteg a joystickon: AgIsoStack++, 8 bemenetes pool, állandó ID-k.
3. `AuxFunctionCatalog.h` + a címke üzenet a **szóróban** (legkevesebb funkció), joystick kijelzés.
4. Vetőgép v2 (mindkét board), permetező.

## Később: idegen gépek (kísérleti)

- A hozzárendelés lehallgatása (TP-n menő Assignment Type 2) → „X gép N-es funkciója”, és a felhasználó egyszer választ
  hozzá ikont a joystick ikonkészletéből; a joystick megjegyzi gép (NAME) + funkció szerint.
- Pool-lehallgatás: ha a joystick épp a buszon van feltöltéskor (első csatlakozás, vagy a terminálon törölt pool),
  az Auxiliary Function Type 2 objektumok és a hozzájuk tartozó képek / szövegek kiszedése és eltárolása.
