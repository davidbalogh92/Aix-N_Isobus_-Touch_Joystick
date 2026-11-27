# Hardver

Fő cél: **3.5" ESP32-32E (E32R35T)**. A 2.8"-os (E32R28T) is támogatott, külön .bin-nel.
Pin kiosztások forrása: lcdwiki.com (3.5inch / 2.8inch ESP32-32E Display).

## 3.5" ESP32-32E (E32R35T) – fő cél

| Funkció | ESP32 láb |
|---|---|
| LCD (ST7796U, 320×480) SCK / MOSI / MISO / DC / CS | 14 / 13 / 12 / 2 / 15 |
| LCD háttérfény | **27** (magas = be) |
| LCD RST | EN (közös az ESP32 resettel) |
| Touch (XPT2046) CS / IRQ | 33 / 36, SCK/MOSI/MISO **közös az LCD-vel** |
| RGB LED R / G / B | 22 / 16 / 17 (közös anód, LOW = világít) |
| Hangszóró engedélyezés / jel | 4 (LOW = be) / 26 |
| BOOT gomb | 0 |
| I2C csatlakozó (1,25 mm 4P) | SCL = IO25, SDA = IO32 |
| Expand bemenetek (1,25 mm 2P) | IO35, IO39 (csak bemenet) |
| SD kártya | 5, 18, 19, 23 (nem használjuk) |
| Fényérzékelő (R21, SMD fotoellenállás) | **IO34** (a felhasználó lapján kimérve; a gyártói leírás akku ADC-nek írja). Szobafényben 0, csak erős sötétben ad jelet (akár maxig) → gyakorlatilag éjjel/nappal váltó. A firmware betanításkor méri ki |

### CAN modul bekötése (3.5")

| SN65HVD230 modul | Lap csatlakozó | ESP32 láb |
|---|---|---|
| TX | I2C csatlakozó **SCL** | **25** |
| RX | Expand **IO35** | **35** |
| 3.3V | I2C csatlakozó 3,3 V lába | – |
| GND | I2C csatlakozó GND | – |

- Egyenes bekötés: modul TX → IO25, modul RX → IO35.
- Az I2C csatlakozón a 3,3 V és a GND is ki van vezetve (4P: táp, GND, SCL, SDA). A pontos sorrendet a lap feliratáról olvasd le, ne a kábelszínekből.
- Szabadon marad: IO32 (I2C SDA) és IO39 (Expand). Ezek jók lesznek később módváltó kapcsolónak / keretfelismerő Hall-szenzornak (az IO39 csak bemenet).

## 2.8" ESP32-32E (E32R28T) – másodlagos

| Funkció | ESP32 láb |
|---|---|
| LCD (ILI9341V; az E32R28T-1-en ST7789) SCK / MOSI / MISO / DC / CS | 14 / 13 / 12 / 2 / 15 |
| LCD háttérfény | 21 |
| Touch (XPT2046) SCK / MOSI / MISO / CS / IRQ | 25 / 32 / 39 / 33 / 36 (külön busz) |
| RGB LED, hang, BOOT | mint a 3.5"-on |
| CAN TX / RX | 27 (SPI csatl. CS) / 35 (Expand) |

## Flash partíciók

`min_spiffs.csv` (v0.7.1-től): 2 × 1,9 MB alkalmazás (OTA), 128 kB SPIFFS. Az NVS (beállítások) a 0x9000 címen van, mint a korábbi `default.csv`-ben, ezért a váltáskor a beállítások megmaradnak. Partíciós tábla csak kábeles (`FULL_0x0.bin`) égetéssel változik.

## Közös tudnivalók

- Csatlakozó: 1,25 mm-es, PicoBlade-kompatibilis (pl. Hestore JT-A1250H-4P-300, -2P-300).
- **120 Ω:** a CAN modulon fix 120 Ω-os ellenállás van. Asztali próbán (csak terminál + joystick) maradhat, traktoron le kell forrasztani.
- Busz sebesség: 250 kbit/s (ISOBUS).
- Táp: asztalon USB-C; traktoron 12 V → 5 V autós step-down tápegység (túlfeszültség-védelemmel). A GND legyen közös a traktor GND-jével.
