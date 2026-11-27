#pragma once

#include <stdint.h>
#include <string.h>

// All access is serialized by the same mutex as the Modbus register map.
enum class WriteResult { accepted, readOnly, invalidValue, busy };
enum class CommandStatus { idle, pending, completed, unknown };
enum class MasterStatus { waiting, fresh, stale };

struct CommandState {
  uint16_t value = 0;
  uint32_t started = 0;
  CommandStatus status = CommandStatus::idle;
  bool mqtt = false;
  bool unconfirmed = false;
  char requestId[65] = {};
};

struct CommandEvent {
  uint16_t address = 0;
  uint16_t value = 0;
  char requestId[65] = {};
};

class GatewayState {
 public:
  static constexpr uint32_t MASTER_STALE_TIMEOUT_MS = 60000;
  WriteResult submit(uint16_t address, uint16_t value, uint16_t& reg,
                     bool mqtt, const char* requestId, uint32_t now) {
    if (address != 100 && address != 101) return WriteResult::readOnly;
    if (!value || (address == 100 && !((value >= 0x0101 && value <= 0x010b) || value == 0x01ff)))
      return WriteResult::invalidValue;
    if (reg != 0) return WriteResult::busy;
    CommandState& command = commands_[address - 100];
    command = CommandState{};
    command.value = value;
    command.started = now;
    command.status = CommandStatus::pending;
    command.mqtt = mqtt;
    if (requestId) {
      strncpy(command.requestId, requestId, sizeof(command.requestId) - 1);
      command.requestId[sizeof(command.requestId) - 1] = '\0';
    }
    reg = value;
    return WriteResult::accepted;
  }

  void masterWrite(uint16_t address, uint16_t oldValue, uint16_t newValue, uint32_t now) {
    if (address == 17 && oldValue != newValue) {
      seenChange_ = true;
      stale_ = false;
      lastChange_ = now;
    }
    if (address != 100 && address != 101) return;
    CommandState& command = commands_[address - 100];
    if (command.status != CommandStatus::pending) return;
    if (oldValue != command.value || (newValue != 0 && newValue != command.value)) {
      command.status = CommandStatus::unknown;
      return;
    }
    if (newValue == 0) {
      command.status = CommandStatus::completed;
      if (command.mqtt && eventCount_ < 4) {
        CommandEvent& event = events_[(eventHead_ + eventCount_) % 4];
        event.address = address;
        event.value = command.value;
        memcpy(event.requestId, command.requestId, sizeof(event.requestId));
        ++eventCount_;
      }
    }
  }

  const CommandState& command(uint16_t address) const { return commands_[address - 100]; }
  bool timedOut(uint16_t address, uint32_t now) const {
    const CommandState& c = command(address);
    return address == 100 && c.value == 0x01ff && c.status == CommandStatus::pending &&
           (c.unconfirmed || static_cast<uint32_t>(now - c.started) >= 10000);
  }
  MasterStatus masterStatus(uint32_t now) const {
    if (stale_ || static_cast<uint32_t>(now - lastChange_) >= MASTER_STALE_TIMEOUT_MS) return MasterStatus::stale;
    return seenChange_ ? MasterStatus::fresh : MasterStatus::waiting;
  }
  void tick(uint32_t now) {
    if (static_cast<uint32_t>(now - lastChange_) >= MASTER_STALE_TIMEOUT_MS) stale_ = true;
    if (timedOut(100, now)) commands_[0].unconfirmed = true;
  }
  bool popCompletion(CommandEvent& event) {
    if (!eventCount_) return false;
    event = events_[eventHead_];
    eventHead_ = (eventHead_ + 1) % 4;
    --eventCount_;
    return true;
  }
 private:
  CommandState commands_[2];
  CommandEvent events_[4];
  uint8_t eventHead_ = 0, eventCount_ = 0;
  bool seenChange_ = false;
  bool stale_ = false;
  uint32_t lastChange_ = 0;
};
