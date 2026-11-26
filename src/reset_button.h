#pragma once
#include <stdint.h>

// Caller samples active-low button; one reset per stable press.
class ResetButton {
 public:
  bool update(bool pressed, uint32_t now) {
    if (pressed != raw_) { raw_ = pressed; changedAt_ = now; }
    if (raw_ != stable_ && uint32_t(now - changedAt_) >= 50) {
      stable_ = raw_;
      if (stable_) { pressedAt_ = now; fired_ = false; }
    }
    if (stable_ && raw_ && !fired_ && uint32_t(now - pressedAt_) >= 3000) {
      fired_ = true;
      return true;
    }
    return false;
  }
 private:
  bool raw_ = false, stable_ = false, fired_ = false;
  uint32_t changedAt_ = 0, pressedAt_ = 0;
};
