# Hardver: 2.8" ESP32-32E (E32R28T) + SN65HVD230

Pin kiosztás forrása: lcdwiki.com, 2.8inch ESP32-32E Display.

## A lapon lévő perifériák

| Funkció | ESP32 láb |
|---|---|
| LCD SCK / MOSI / MISO / DC / CS | 14 / 13 / 12 / 2 / 15 |
| LCD háttérfény | 21 (magas = be) |
| LCD RST | EN (közös az ESP32 resettel) |
| Touch (XPT2046) SCK / MOSI / MISO / CS / IRQ | 25 / 32 / 39 / 33 / 36 |
| RGB LED R / G / B | 22 / 16 / 17 (közös anód, LOW = világít) |
| Hangszóró engedélyezés / jel | 4 (LOW = be) / 26 |
| BOOT gomb | 0 |
| SD kártya | 5, 18, 19, 23 (**nem használjuk**, mert a 4P SPI csatlakozóval közös) |

## CAN modul bekötése

| SN65HVD230 modul | Lap csatlakozó | ESP32 láb |
|---|---|---|
| 3.3V | Expand (1,25 mm) 3,3 V | – |
| GND | Expand GND | – |
| RX | Expand IO35 | 35 (csak bemenet) |
| TX | SPI periféria (4P, 1,25 mm) „CS” | 27 |

- Egyenes bekötés: modul TX → ESP TX láb (27), modul RX → ESP RX láb (35).
- A csatlakozó lábsorrendjét a lap feliratáról olvasd le, ne a kábelszínekből.
- Csatlakozó: 1,25 mm-es, PicoBlade-kompatibilis (pl. Hestore JT-A1250H-4P-300 / -2P vagy -3P).
- **120 Ω:** a modulon fix 120 Ω-os ellenállás van. Asztali próbán (csak terminál + joystick) maradhat, traktoron le kell forrasztani.
- Busz sebesség: 250 kbit/s (ISOBUS).

## Táp

- Asztalon USB-C.
- Traktoron 12 V → 5 V autós step-down tápegység (túlfeszültség-védelemmel) a lap 5 V / USB-C bemenetére. A GND legyen közös a traktor GND-jével.
