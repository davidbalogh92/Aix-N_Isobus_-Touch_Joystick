#include "muller_out.h"

static constexpr uint32_t PGN_ADDRESS_CLAIM = 0xEE00;
static constexpr uint32_t PGN_REQUEST       = 0xEA00;

uint64_t MullerAuxOSender::makeName(uint32_t identity) {
  uint64_t n = 0;
  n |= (uint64_t)(identity & 0x1FFFFF);        // 0..20  identity
  n |= (uint64_t)98 << 21;                     // 21..31 gyártó (Müller)
  n |= (uint64_t)0 << 32;                      // 32..34 ECU instance
  n |= (uint64_t)0 << 35;                      // 35..39 function instance
  n |= (uint64_t)128 << 40;                    // 40..47 function
  n |= (uint64_t)13 << 49;                     // 49..55 device class
  n |= (uint64_t)0 << 56;                      // 56..59 device class instance
  n |= (uint64_t)2 << 60;                      // 60..62 industry group
  n |= (uint64_t)1 << 63;                      // 63     self-configurable
  return n;
}

void MullerAuxOSender::begin(uint32_t identity, uint8_t preferredAddress, uint32_t now) {
  _name = makeName(identity);
  _preferred = preferredAddress;
  _sa = preferredAddress;
  _active = NO_INPUT;
  for (auto& m : _usedMask) m = 0;
  _state = State::Claiming;
  _claimT0 = now;
  _lastKeepalive = now;
  sendClaim();
}

void MullerAuxOSender::stop(uint32_t now) {
  release(now);
  _state = State::Off;
}

void MullerAuxOSender::sendClaim() {
  if (!send) return;
  uint8_t d[8];
  for (int i = 0; i < 8; i++) d[i] = (uint8_t)(_name >> (8 * i));
  uint8_t sa = (_state == State::CannotClaim) ? 0xFE : _sa;
  send(0x18EEFF00UL | sa, d, 8);
}

void MullerAuxOSender::sendStatus(uint8_t input, uint8_t ss) {
  if (!send) return;
  uint8_t d[8] = {0x21, input, 0xFF, 0xFF, _cc[input], 0x00, ss, 0xFF};
  send(0x1CE6FF00UL | _sa, d, 8);
}

void MullerAuxOSender::sendKeepalive() {
  if (!send) return;
  uint8_t d[8] = {0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  send(0x1CE7FF00UL | _sa, d, 8);
}

bool MullerAuxOSender::pickNextAddress() {
  // dinamikus tartomány 128..247, a kért címtől felfelé körbe
  for (int k = 1; k <= 120; k++) {
    uint8_t cand = (uint8_t)(128 + ((_sa - 128 + k) % 120));
    if (cand == _sa) continue;
    if (!(_usedMask[cand >> 5] & (1UL << (cand & 31)))) {
      _sa = cand;
      return true;
    }
  }
  return false;
}

void MullerAuxOSender::handleFrame(uint32_t id, const uint8_t* d, uint8_t len, uint32_t now) {
  if (_state == State::Off) return;
  uint8_t  sa  = id & 0xFF;
  uint8_t  pf  = (id >> 16) & 0xFF;
  uint8_t  ps  = (id >> 8) & 0xFF;
  uint32_t pgn = (pf >= 240) ? ((id >> 8) & 0x3FFFF) : ((uint32_t)pf << 8);

  if (pgn == PGN_ADDRESS_CLAIM && len >= 8) {
    uint64_t other = 0;
    for (int i = 7; i >= 0; i--) other = (other << 8) | d[i];
    if (other == _name) return;                 // saját visszhang
    if (sa < 0xFE) _usedMask[sa >> 5] |= (1UL << (sa & 31));
    if (sa == _sa && _state != State::CannotClaim) {
      if (other < _name) {
        // ő a magasabb prioritású: elengedünk, új címet keresünk
        _active = NO_INPUT;
        if (pickNextAddress()) {
          _state = State::Claiming;
          _claimT0 = now;
        } else {
          _state = State::CannotClaim;
        }
        sendClaim();
      } else {
        sendClaim();                            // mi nyerünk: újra bejelentjük
      }
    }
    return;
  }

  if (pgn == PGN_REQUEST && len >= 3) {
    uint32_t req = (uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16);
    if (req == PGN_ADDRESS_CLAIM && (ps == 0xFF || ps == _sa)) sendClaim();
  }
}

bool MullerAuxOSender::press(uint8_t input, uint32_t now) {
  if (input >= NUM_INPUTS) return false;
  if (_active != NO_INPUT) release(now);
  if (_state != State::Claimed) return false;
  _cc[input]++;
  _active = input;
  _lastHoldTx = now;
  sendStatus(input, 1);
  return true;
}

void MullerAuxOSender::release(uint32_t now) {
  (void)now;
  if (_active == NO_INPUT) return;
  uint8_t in = _active;
  _active = NO_INPUT;
  if (_state == State::Claimed) sendStatus(in, 0);
}

void MullerAuxOSender::update(uint32_t now) {
  if (_state == State::Claiming && now - _claimT0 >= CLAIM_WAIT_MS) {
    _state = State::Claimed;
    _lastKeepalive = now - KEEPALIVE_MS;        // azonnal menjen egy életjel
  }
  if (_state != State::Claimed) return;

  if (_active != NO_INPUT && now - _lastHoldTx >= HOLD_REPEAT_MS) {
    _lastHoldTx = now;
    sendStatus(_active, 2);
  }
  if (now - _lastKeepalive >= KEEPALIVE_MS) {
    _lastKeepalive = now;
    sendKeepalive();
  }
}
