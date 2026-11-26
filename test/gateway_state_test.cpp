#include "gateway_state.h"
#include <cassert>
#include <cstring>

int main() {
  for (uint16_t code = 0x0101; code <= 0x010b; ++code) {
    GatewayState candidate;
    uint16_t reg = 0;
    assert(candidate.submit(100, code, reg, false, "", 0) == WriteResult::accepted);
    assert(reg == code);
  }
  GatewayState state;
  uint16_t r100 = 0, r101 = 0;
  assert(state.submit(0, 1, r100, false, "", 0) == WriteResult::readOnly);
  assert(state.submit(100, 0x0100, r100, false, "", 0) == WriteResult::invalidValue);
  assert(state.submit(100, 0x010c, r100, false, "", 0) == WriteResult::invalidValue);
  assert(r100 == 0);
  assert(state.submit(101, 0, r101, false, "", 0) == WriteResult::invalidValue);
  assert(state.submit(100, 0x01ff, r100, true, "test", 100) == WriteResult::accepted);
  assert(state.submit(100, 0x0102, r100, false, "", 100) == WriteResult::busy);
  assert(state.submit(101, 65535, r101, false, "", 100) == WriteResult::accepted);
  assert(state.command(100).status == CommandStatus::pending);
  assert(!state.timedOut(100, 10099));
  assert(state.timedOut(100, 10100));
  state.masterWrite(100, r100, 0, 10200);
  r100 = 0;
  assert(state.command(100).status == CommandStatus::completed);
  // A new command can arrive before the MQTT event loop has sent the old completion.
  assert(state.submit(100, 0x0102, r100, false, "", 10300) == WriteResult::accepted);
  CommandEvent event;
  assert(state.popCompletion(event));
  assert(event.address == 100 && event.value == 0x01ff && !strcmp(event.requestId, "test"));
  assert(!state.popCompletion(event));
  state.masterWrite(100, r100, 0x0103, 10301);
  r100 = 0x0103;
  state.masterWrite(100, r100, 0, 10302);
  assert(state.command(100).status == CommandStatus::unknown);
  assert(!state.popCompletion(event));
  assert(state.masterStatus(0) == MasterStatus::waiting);
  assert(state.masterStatus(10000) == MasterStatus::waiting);
  assert(state.masterStatus(10001) == MasterStatus::stale);
  state.masterWrite(17, 0, 1, 10002);
  assert(state.masterStatus(10002) == MasterStatus::fresh);
  state.masterWrite(17, 1, 1, 15000);
  assert(state.masterStatus(20003) == MasterStatus::stale);
  state.masterWrite(17, 1, 2, 20004);
  assert(state.masterStatus(20004) == MasterStatus::fresh);

  GatewayState wrap;
  wrap.masterWrite(17, 0, 1, 0xfffffff0U);
  assert(wrap.masterStatus(10) == MasterStatus::fresh);
  wrap.tick(10001);
  assert(wrap.masterStatus(10001) == MasterStatus::stale);
  wrap.tick(0xfffffff0U);
  assert(wrap.masterStatus(5) == MasterStatus::stale);
  uint16_t reg = 0;
  assert(wrap.submit(100, 0x01ff, reg, false, "", 0xfffffff0U) == WriteResult::accepted);
  wrap.tick(10000);
  assert(wrap.timedOut(100, 5));
  wrap.masterWrite(100, reg, 0, 10001);
  assert(!wrap.timedOut(100, 10001));
  return 0;
}
