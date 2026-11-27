#pragma once
#include <stdint.h>

class HumidityControl {
 public:
  void failed() { active_ = false; }
  bool update(bool fresh, bool enabled, uint16_t humidity, uint8_t limit,
              bool repeat, uint16_t minutes, uint32_t now) {
    if (!enabled) { active_ = false; return false; }
    if (!fresh) return false;
    if (humidity <= limit) { active_ = false; return false; }
    if (!active_ || (repeat && uint32_t(now - lastSent_) >= uint32_t(minutes) * 60000UL)) {
      active_ = true;
      lastSent_ = now;
      return true;
    }
    return false;
  }
 private:
  bool active_ = false;
  uint32_t lastSent_ = 0;
};
