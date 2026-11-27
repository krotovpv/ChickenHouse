#pragma once

#include <ArduinoJson.h>
#include <string.h>

inline bool readRequestId(JsonVariantConst field, const char*& result) {
  result = nullptr;
  if (field.isNull()) return true;
  if (!field.is<const char*>()) return false;
  JsonString value = field.as<JsonString>();
  if (value.size() > 64 || memchr(value.c_str(), '\0', value.size()) != nullptr) return false;
  result = value.c_str();
  return true;
}
