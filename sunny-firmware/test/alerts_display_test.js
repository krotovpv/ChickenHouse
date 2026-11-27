const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const source = fs.readFileSync('src/web_pages.cpp', 'utf8');
const start = source.indexOf('  function activeAlarmDescriptions(data)');
const end = source.indexOf('  // END activeAlarmDescriptions', start);
assert(start >= 0 && end > start);
const alarms = vm.runInNewContext('(' + source.slice(start, end).trim() + ')');
assert.equal(alarms({1: 0, 3: 0}).length, 0);
assert.deepEqual(Array.from(alarms({1: (1 << 9) | (1 << 10), 3: (1 << 5) | (1 << 9), 48: 2, 52: 1})), [
  'Авария батарейки', 'Авария датчика температуры', 'Ошибка расчёта вентиляции',
  'Ошибка при кормлении', 'Ошибки кормушки К1: 2', 'Ошибки модуля Wi-Fi: 1'
]);
assert.equal(alarms({1: 1 << 5}).length, 0);
assert(!source.includes("id='heater-sensor-alarm'"));
assert(!source.includes("id='heater-emergency-mode'"));
assert(source.includes("id='b_1_5'"));
