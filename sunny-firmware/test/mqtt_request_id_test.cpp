#include "mqtt_command_input.h"
#include <cassert>
#include <string>

static bool validId(const std::string& json) {
  JsonDocument doc;
  assert(!deserializeJson(doc, json.c_str()));
  const char* id = nullptr;
  return readRequestId(doc["request_id"], id);
}

static bool validMutableId(std::string json) {
  JsonDocument doc;
  assert(!deserializeJson(doc, &json[0], json.size()));
  const char* id = nullptr;
  return readRequestId(doc["request_id"], id);
}

int main() {
  assert(validId("{}"));
  assert(validId("{\"request_id\":\"cmd-1\"}"));
  assert(!validId("{\"request_id\":123}"));
  assert(!validId("{\"request_id\":\"cmd\\u0000other\"}"));
  assert(!validMutableId("{\"request_id\":\"cmd\\u0000other\"}"));
  assert(validId("{\"request_id\":\"" + std::string(64, 'a') + "\"}"));
  assert(!validId("{\"request_id\":\"" + std::string(65, 'a') + "\"}"));
  return 0;
}
