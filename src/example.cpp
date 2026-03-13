#include <Arduino.h>
#include <map>

// --- чтение байта ---

// -- первый вариант --
// uint16_t regValue = memo[100]; // Допустим, регистр 100 содержит флаги

// bool isLightOn   = (regValue & (1 << 0)); // Проверка 0-го бита
// bool isFanActive = (regValue & (1 << 1)); // Проверка 1-го бита
// bool isError     = (regValue & (1 << 15)); // Проверка 15-го бита

// -- второй вариант --
// struct StatusFlags {
//     uint16_t pumpActive : 1;  // 1 бит
//     uint16_t valveOpen  : 1;  // 1 бит
//     uint16_t reserved   : 6;  // Пропуск 6 бит
//     uint16_t mode       : 4;  // Например, 4 бита под режим работы
//     uint16_t criticalErr: 1;  // 1 бит
// };

// // Распаковка:
// uint16_t regValue = memo[10]; 
// StatusFlags* flags = (StatusFlags*)&regValue;

// if (flags->pumpActive) {
//     // Делаем что-то, если насос включен
// }

// -- тредтий вариант --
// #define CHECK_BIT(reg, pos) (((reg) >> (pos)) & 1)

// // Использование:
// bool bit5 = CHECK_BIT(memo[0], 5);