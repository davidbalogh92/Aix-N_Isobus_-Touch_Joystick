#include "can_monitor.h"
#include "config.h"
#include "driver/twai.h"

static CanStats s_st;
static bool     s_driverOk     = false;
static uint32_t s_winStart     = 0;
static uint32_t s_winCount     = 0;
static uint32_t s_lastRxMs     = 0;
static uint32_t s_lastVtMs     = 0;
static uint32_t s_nodeMask[8]  = {0};   // 256 bites címtérkép
static uint32_t s_nodeWinStart = 0;

static const uint32_t PGN_VT_STATUS = 0xFE6E;

bool canBegin() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
      (gpio_num_t)PIN_CAN_TX, (gpio_num_t)PIN_CAN_RX, TWAI_MODE_NORMAL);
  g.rx_queue_len = 64;
  g.tx_queue_len = 8;
  twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS();   // ISOBUS: 250 kbit/s
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g, &t, &f) != ESP_OK) {
    Serial.println("[CAN] driver install HIBA");
    s_st.state = CanState::DriverFail;
    return false;
  }
  if (twai_start() != ESP_OK) {
    Serial.println("[CAN] start HIBA");
    s_st.state = CanState::DriverFail;
    return false;
  }
  s_driverOk = true;
  s_st.state = CanState::NoTraffic;
  s_winStart = s_nodeWinStart = millis();
  Serial.printf("[CAN] OK, 250k, TX=%d RX=%d\n", PIN_CAN_TX, PIN_CAN_RX);
  return true;
}

static uint8_t countBits() {
  uint8_t n = 0;
  for (int i = 0; i < 8; i++) n += __builtin_popcount(s_nodeMask[i]);
  return n;
}

void canLoop() {
  if (!s_driverOk) return;
  uint32_t now = millis();

  // ---- beérkező frame-ek feldolgozása ----
  twai_message_t m;
  int guard = 0;
  while (guard++ < 64 && twai_receive(&m, 0) == ESP_OK) {
    s_winCount++;
    s_st.totalRx++;
    s_lastRxMs = now;
    if (m.extd) {
      uint32_t id = m.identifier;
      uint8_t sa  = id & 0xFF;
      uint8_t pf  = (id >> 16) & 0xFF;
      uint32_t pgn = (pf >= 240) ? ((id >> 8) & 0x3FFFF) : ((id >> 8) & 0x3FF00);
      s_nodeMask[sa >> 5] |= (1UL << (sa & 31));
      if (pgn == PGN_VT_STATUS) {
        s_lastVtMs = now;
        s_st.vtAddr = sa;
      }
    }
  }

  // ---- statisztika ablakok ----
  if (now - s_winStart >= 1000) {
    s_st.fps = s_winCount * 1000 / (now - s_winStart);
    s_winCount = 0;
    s_winStart = now;
  }
  if (now - s_nodeWinStart >= 2000) {
    s_st.nodes = countBits();
    memset(s_nodeMask, 0, sizeof(s_nodeMask));
    s_nodeWinStart = now;
  }
  s_st.vtSeen = (s_lastVtMs != 0) && (now - s_lastVtMs < 3000);

  // ---- busz állapot ----
  twai_status_info_t si;
  if (twai_get_status_info(&si) == ESP_OK) {
    s_st.txErr = si.tx_error_counter;
    s_st.rxErr = si.rx_error_counter;
    if (si.state == TWAI_STATE_BUS_OFF) {
      s_st.state = CanState::BusOff;
      twai_initiate_recovery();
      return;
    }
    if (si.state == TWAI_STATE_RECOVERING) {
      s_st.state = CanState::BusOff;
      return;
    }
    if (si.state == TWAI_STATE_STOPPED) {
      // helyreállítás után újra kell indítani
      twai_start();
      s_st.state = CanState::NoTraffic;
      return;
    }
    if (si.tx_error_counter >= 128 || si.rx_error_counter >= 128) {
      s_st.state = CanState::ErrorPassive;
      return;
    }
  }
  s_st.state = (s_lastRxMs != 0 && now - s_lastRxMs < 1500) ? CanState::Ok : CanState::NoTraffic;
}

const CanStats& canStats() { return s_st; }

const char* canStateText(CanState s) {
  switch (s) {
    case CanState::DriverFail:   return "CAN: driver hiba";
    case CanState::NoTraffic:    return "CAN: nincs forgalom";
    case CanState::Ok:           return "CAN: OK";
    case CanState::ErrorPassive: return "CAN: hibás busz";
    case CanState::BusOff:       return "CAN: BUS-OFF";
  }
  return "CAN: ?";
}
