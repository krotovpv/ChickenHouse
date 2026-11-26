#include "../src/reset_button.h"
#include <cassert>
int main() {
  ResetButton b;
  assert(!b.update(false, 0));
  assert(!b.update(true, 100));
  assert(!b.update(false, 110));
  assert(!b.update(true, 200));
  assert(!b.update(true, 250));
  assert(!b.update(true, 3249));
  assert(b.update(true, 3250));
  assert(!b.update(true, 5000));
  assert(!b.update(false, 5100));
  assert(!b.update(false, 5150));
  assert(!b.update(true, 5200));
  assert(!b.update(true, 5250));
  assert(!b.update(false, 6000));
  assert(!b.update(false, 6050));
  assert(!b.update(true, 7000));
  assert(!b.update(true, 7050));
  assert(b.update(true, 10050));
}
