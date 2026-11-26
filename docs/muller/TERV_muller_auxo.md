# Terv: Müller AUX-O kimenet az AgIsoAuxInputs joystickba

Készítette: Balogh Dávid & Claude, 2026-11-25

## Cél

A home made AUX-N joystick tudjon a gyári Müller permetező Jobcomputernek is parancsot adni, a
Müller joystick (gyári permetező kiosztás) üzeneteivel. A VT-n egy új oldalon gombonként be lehessen
állítani, melyik joystick gomb melyik Müller funkciót küldje.

## Bemért és kipróbált tények (CANable + Python emulátor, valódi Müller Jobcomputerrel)

- A Müller joystick AUX-O (Auxiliary Input Type 1) állapotüzenetet küld:
  - CAN ID `0x1CE6FFxx` = prioritás 7, PGN 0xE600 (VT->ECU), cél: global (0xFF), xx = küldő címe
  - adat: `21 nn FF FF cc 00 ss FF`
    - `nn` = gombszám 0..23 (lásd táblázat)
    - `cc` = számláló-szerű bájt; a valódi joysticknál gombonként más, az emulátor lenyomásonként
      eggyel növelte, a Jobcomputer így elfogadta
    - `ss` = 0 elengedve, 1 lenyomva (egyszer, a lenyomás pillanatában), 2 tartva (200 ms-onként, amíg tartja)
- **A Jobcomputer a NAME alapján fogadja el az üzenetet, a címtől függetlenül:**
  - Müller-szerű NAME, 0x90 cím: MŰKÖDIK
  - home made joystick NAME (gyártó 1407, function 129, class 0), 0xCF cím: NEM működik
  - Müller-szerű NAME, 0xCF cím: MŰKÖDIK
- A működő NAME (emulátor): `A01A80000C5ABCDE`
  - self-configurable 1, industry group 2, device class 13, device class instance 0,
    function 128, function instance 0, ECU instance 0, manufacturer 98, identity 0x1ABCDE
  - a valódi Müller joystick identity-je 0x10C71D; eltérő identity-vel a kettő egyszerre lehet a buszon
- Az emulátor ezen felül másodpercenként küldött egy `ECU->VT` working set maintenance-szerű keretet
  (PGN 0xE700, global, `FF 00 FF FF FF FF FF FF`), és válaszolt az Address Claim Request-re.
  Nem tudjuk, hogy a maintenance keret kell-e a Jobcomputernek; első körben ugyanígy kell csinálni,
  mint az emulátor (bizonyítottan működik), és csak teszt után szabad elhagyni.
- AUX-kiosztó (assignment) üzenet nem megy a termináltól: a Jobcomputer fix gombszámokat hallgat.

## Müller gombszámok (gombszám = váltóállás*8 + belső index; a joystick oszloponként számoz)

| nn | Funkció | nn | Funkció | nn | Funkció |
|---|---|---|---|---|---|
| 0 | TRAIL kézi/auto | 8 | Célérték 100% | 16 | Keret kézi/auto |
| 1 | TRAIL be/ki | 9 | Szakasz-főkapcsoló | 17 | Keret szabad gomb |
| 2 | TRAIL szabad (B3) | 10 | Szakaszok be jobbról balra | 18 | Keret emelés |
| 3 | TRAIL szabad (B5) | 11 | Szakaszok ki balról jobbra | 19 | Keret süllyesztés |
| 4 | TRAIL szabad (B4) | 12 | Szakaszok be balról jobbra | 20 | Keret behajlítás |
| 5 | TRAIL szabad (B6) | 13 | Szakaszok ki jobbról balra | 21 | Keret kihajlítás |
| 6 | Kormányzás balra | 14 | Adag - | 22 | Bal fel, jobb le |
| 7 | Kormányzás jobbra | 15 | Adag + | 23 | Jobb fel, bal le |

## Megvalósítás

### 1. Második control function („Müller joystick”)

- A meglévő AUX-N internal control function marad változatlanul (NAME, pool, assignment nem változik).
- Új internal control function a Müller-szerű NAME-mel (gyártó 98, function 128, device class 13,
  industry group 2, self-configurable, **saját identity**, pl. 0x1ABCDE), tetszőleges kért címmel
  (pl. 0x90); a címfoglalást és a Request-re adott választ az AgIsoStack intézi.
- Csak akkor jön létre / jelentkezik be, ha a Müller mód be van kapcsolva (Beállításokban), hogy
  más gépeken ne jelenjen meg egy idegen Müller eszköz a buszon.
- A pontos AgIsoStack API-t (InternalControlFunction létrehozás, nyers PGN küldés global célra,
  prioritás 7) a projektben használt verzióban kell ellenőrizni; a `patch_agisostack.py` miatt
  figyelni, hogy a patch-elt stack ezt ne törje.

### 2. Küldés

- A gombkezelés (aux_buttons) minden fizikai gomb változásánál, ha a gombhoz Müller funkció van rendelve:
  - lenyomás: azonnal `ss=1`, és a gomb `cc` számlálója +1
  - tartás: 200 ms-onként `ss=2`
  - elengedés: azonnal `ss=0`
- Másodpercenként a `0xE700 FF 00 FF ...` keret a Müller CF-ről (lásd fent, teszt után elhagyható-e).
- Ha a Müller CF nem birtokol érvényes címet, ne küldjön.
- Az AUX-N küldés ettől függetlenül fut tovább (mód: AUX-N / Müller / mindkettő).
- Biztonság: a Jobcomputer a tartott funkciót a 200 ms-os ismétlés elmaradására állítja le, ezért az
  ismétlést nem szabad blokkoló kód mögé tenni (külön időzítő vagy a fő loop elején).

### 3. Beállítás (settings, NVS)

- `muller_mode`: 0 = ki, 1 = csak Müller, 2 = AUX-N + Müller
- `muller_map[16]`: fizikai gombonként 0..23 Müller gombszám, vagy 0xFF = nincs
- `muller_identity`: alapból 0x1ABCDE, és `muller_sa`: alapból 0x90 (haladó beállítás)
- Analóg tengelyek: első körben nem küldenek Müller üzenetet.

### 4. VT oldal („MÜLLER”)

- Új mask a `build_aux_pool.py`-ban, a Settings oldal mintájára; elérés a Settings oldalról egy gombbal
  vagy saját softkey-jel.
- Soronként egy fizikai gomb (a gomb neve/száma, ahogy a főképernyőn), mellette Input List vagy
  léptető gomb 25 elemmel: „nincs” + a 24 funkció neve a fenti táblázatból.
- Egy sor a módhoz (Ki / Müller / AUX-N + Müller), és egy rövid szöveg: „Az új mód újraindítás után érvényes”,
  ha a CF létrehozása újraindítást igényel.
- A pool mérete és a régi terminálok (VT3) miatt a stringek legyenek rövidek; a `check_pool_scaling.py`
  ellenőrzést lefuttatni.
- Opcionális: ugyanez a beállítás az AP-módú weboldalon is.

### 5. Későbbi bővítés (nem első kör)

- Multi-page mód rákötése a Müller szintekre (T1 = TRAIL, T2 = permetezés, T3 = keret), hogy ne kelljen
  24 funkcióból egyenként választani.

## Tesztelés

1. CANable + `joystick_bemero.py --aux` (vagy PCAN-View): a home made joystick Müller CF-je bejelentkezik-e
   (Address Claim a Müller-szerű NAME-mel), és gombnyomásra a fenti kereteket küldi-e, helyes időzítéssel.
2. Valódi Müller terminál + Jobcomputer, a valódi Müller joystick kihúzva: minden beállított gomb a jó
   funkciót váltja-e (a `seq`-hez hasonlóan végignyomva).
3. Biztonsági teszt: tartott keretfunkció közben a joystick CAN-jét bontani -> a Jobcomputernek le kell
   állítania a funkciót.
4. Együttélés: valódi Müller joystick és home made joystick egyszerre a buszon -> mindkettő működik,
   nincs címütközés.
5. Visszalépés: Müller mód kikapcsolva -> a Müller CF nem jelenik meg a buszon, AUX-N változatlanul működik.

## Kapcsolódó eszközök

- `joystick_emulator.py`: a bizonyítottan működő referencia (NAME, keretek, időzítés)
- `joystick_bemero.py --aux`: élő AUX-O kiírás, a home made joystick kimenetének ellenőrzésére
- `MullerJoystickAuxO.h`: vevő oldali dekóder (a saját gépvezérlőkhöz), ugyanezzel a táblázattal
