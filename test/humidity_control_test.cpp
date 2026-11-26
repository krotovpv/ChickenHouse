#include "../src/humidity_control.h"
#include <cassert>
int main() {
  HumidityControl state;
  assert(!state.update(true, true, 95, 95, false, 30, 0));
  assert(state.update(true, true, 96, 95, false, 30, 1));
  assert(!state.update(true, true, 96, 95, false, 30, 2000000));
  assert(!state.update(true, true, 95, 95, false, 30, 2000001));
  assert(!state.update(false, true, 96, 95, true, 30, 2000002));
  assert(state.update(true, true, 96, 95, true, 30, 2000003));
  assert(!state.update(true, true, 96, 95, true, 30, 3800002));
  assert(state.update(true, true, 96, 95, true, 30, 3800003));
  assert(!state.update(true, false, 96, 95, true, 30, 4000000));
}
