#pragma once
// WiFi OTA frissítés: saját hozzáférési pont (csak a Frissítés képernyőn), böngészős feltöltő oldal.
// A WiFi alapból KI van kapcsolva; otaStart() kapcsolja be, otaStop() ki.
// Csak alkalmazás képet fogad (..._APP_OTA.bin, első bájt 0xE9); a ..._FULL_0x0.bin-t elutasítja.
#include <Arduino.h>

enum class OtaState : uint8_t { Off, Ready, Receiving, Done, Error };

struct OtaStatus {
  OtaState state = OtaState::Off;
  uint32_t received = 0;     // eddig fogadott bájt
  uint32_t total = 0;        // a fájl mérete (ha a böngésző megadta), 0 = ismeretlen
  uint8_t  clients = 0;      // csatlakozott eszközök
  uint32_t lastActivityMs = 0;
  char     ssid[24] = "";
  char     pass[16] = "";
  char     msg[64] = "";     // hibaüzenet / fájlnév
};

void otaStart();
void otaStop();
void otaLoop();                 // a Frissítés képernyő ciklusából
const OtaStatus& otaStatus();
