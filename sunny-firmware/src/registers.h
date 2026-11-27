#pragma once
#include <Arduino.h>
#include <map>

// Массив памяти Modbus-регистров шлюза <Адрес_Регистра, Значение>
extern std::map<uint16_t, uint16_t> memo;