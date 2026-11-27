#pragma once
#include <Arduino.h>

// Static table resources. Keep commands and register display semantics unchanged.
const char tableStyle[] PROGMEM = R"tableasset(
        .flash-update { background-color: #8ce757 !important; transition: background-color 2s; }
        .bit-row { 
            display: grid; 
            grid-template-columns: repeat(8, 1fr); 
            gap: 4px; 
            margin-top: 8px; 
            padding: 5px;
            background: #f9f9f9;
            border-radius: 4px;
        }
        .bit-dot { 
            width: 10px; height: 10px; border-radius: 50%; 
            background: #ddd; border: 1px solid #ccc;
            margin: 0 auto; position: relative;
        }
        .bit-on { background: #28a745; box-shadow: 0 0 5px #28a745; border-color: #1e7e34; }
        .bit-label { font-size: 12px; color: #64748b; display: block; text-align: center; }

        .switch { position: relative; display: inline-block; width: 48px; height: 28px; vertical-align: middle; margin-left: 10px; flex: 0 0 48px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 20px; }
        .slider:before { position: absolute; content: ""; height: 20px; width: 20px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }
        input:checked + .slider { background-color: #28a745; }
        input:checked + .slider:before { transform: translateX(20px); }
        .reg-label { font-weight: bold; font-size: 14px; }
        @keyframes flash-green {
          0% { background-color: rgba(40, 167, 69, 0.7); }
          100% { background-color: transparent; }
        }
        .flash-active {
          animation: flash-green 1s ease-out;
        }

.register-summary{display:grid;grid-template-columns:2fr 1fr;gap:12px;align-items:start}.register-summary>*{min-width:0;overflow-wrap:anywhere}.register-card{border-top:5px solid #c29246;border-radius:16px;box-sizing:border-box;width:100%}#regTable{display:table;width:100%;table-layout:fixed;border-collapse:collapse;background:#fffefa}#regTable th,#regTable td{border:0;border-bottom:1px solid #ece3d5;text-align:left;padding:12px;vertical-align:top}#regTable th{background:#fff3d9;color:#6d5742;font-size:12px;text-transform:none}#regTable th:first-child,#regTable tr>td:first-child{width:82px}#regTable .val-cell{overflow-wrap:anywhere}#regTable .register-title{font-weight:700;color:#503f32;margin-bottom:5px}#regTable .register-value{font-weight:700;color:#654828}#regTable details{margin-top:10px;padding:0 12px 12px;border:1px solid #b4d7c7;background:#e3f1ec;border-radius:10px}#regTable summary{cursor:pointer;padding:11px 0;color:#255b4c;font-size:14px;font-weight:700}#regTable .register-flags{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:7px}#regTable .register-flag{display:flex;justify-content:space-between;align-items:center;gap:8px;min-height:44px;padding:8px 10px;background:#fbfffc;border:1px solid #c6ded1;border-radius:8px;color:#344e41}#regTable .register-flag.active{background:#d1edda;border-color:#86b89a}#regTable .register-flag small{color:#60796b;margin-right:5px}#regTable .register-flag b{min-width:22px;text-align:center}#register-search{max-width:100%}#register-error{color:#a2362b}#regTable tr[hidden]{display:none!important}
@media(max-width:650px){#regTable,#regTable tbody{display:block;width:100%}#regTable thead{display:none}#regTable tr{display:block;padding:10px 0;border-bottom:1px solid #eddfcc}#regTable tr>td{display:block;width:100%;box-sizing:border-box;border:0;padding:5px 8px}#regTable .register-flags{grid-template-columns:1fr}#regTable tr[hidden]{display:none!important}}
)tableasset";

const char tableScript[] PROGMEM = R"tableasset(

    const regTitles = {
      "0": "Основные статусы",
      "1": "Режимы вентиляции и дверей",
      "2": "Управление лазом и календарем",
      "3": "Ошибки и кормление",
      "4": "Статус подключенных модулей (K1-K4)",
      "5": "Измеренная температура курятника",
      "6": "Влажность курятника",
      "7": "Температура на улице",
      "8": "Влажность на улице",
      "9": "Время закрытия лаза (расчетное)",
      "10": "Время открытия лаза (расчетное)",
      "11": "Резерв",
      "12": "Номер кормушки следующего кормления",
      "13": "Длительность следующего кормления",
      "14": "Количество прошедших кормлений за сутки",
      "15": "Запрограммированое количество кормлений",
      "16": "Время следующего кормления",
      "17": "Секунда",
      "18": "Минута",
      "19": "Час",
      "20": "Число",
      "21": "Месяц",
      "22": "Год",
      "23": "Резерв",
      "24": "Заданная температура",
      "25": "Отклонение температуры",
      "26": "Заданная влажность",
      "27": "Отклонение влажности",
      "28": "Количество датчиков температуры",
      "29": "Заданная температура днем",
      "30": "Температура охлаждения",
      "31": "\"Газ\" количесво сработок в час",
      "32": "\"Газ\" общее число сработок",
      "33": "Температура открытия лаза",
      "34": "Количество включений вентиляции в час",
      "35": "Расчитанное время смены объема воздуха",
      "36": "Время восхода солнца",
      "37": "Время заката солнца",
      "38": "Записаное время подъема",
      "39": "Записаное время отбоя",
      "40": "Записаное время открытия лаза",
      "41": "Записаное время закрытия лаза",
      "42": "Время отложенного закрытия лаза",
      "43": "Время движения привода лаза",
      "44": "Количество включений вентиляторов в час при загазованности",
      "45": "Время дежурного освещения",
      "46": "Время сумерек",
      "47": "Среднее время нагревателя в часу",
      "48": "Количество ошибок по кормушке К1",
      "49": "Количество ошибок по кормушке К2",
      "50": "Количество ошибок по кормушке К3",
      "51": "Количество ошибок по кормушке К4",
      "52": "Ошибки от модуля Wi-Fi",
      "53": "Количество используемых вентиляторов",
      "54": "Производительность вентиляторов",
      "55": "Объем курятника",
      "56": "Масса одной птицы",
      "57": "Количество птиц",
      "58": "Допустимое количество проветриваний в час",
      "59": "Используемый язык в приборе",
      "60": "Лето - объем свежего воздуха ГОСТ",
      "61": "Зима - объем свежего воздуха ГОСТ",
      "62": "Осень - объем свежего воздуха ГОСТ",
      "63": "Лето - объем свежего воздуха",
      "64": "Зима - объем свежего воздуха",
      "65": "Осень - объем свежего воздуха",
      "66": "Резерв",
      "67": "Резерв",
      "68": "Резерв",
      "69": "Резерв",
      "70": "Резерв",
      "71": "Резерв",
      "72": "Январь восход",
      "73": "Февраль восход",
      "74": "Март восход",
      "75": "Апрель восход",
      "76": "Май восход",
      "77": "Июнь восход",
      "78": "Июль восход",
      "79": "Август восход",
      "80": "Сентябрь восход",
      "81": "Октябрь восход",
      "82": "Ноябрь восход",
      "83": "Декабрь восход",
      "84": "Январь закат",
      "85": "Февраль закат",
      "86": "Март закат",
      "87": "Апрель закат",
      "88": "Май закат",
      "89": "Июнь закат",
      "90": "Июль закат",
      "91": "Август закат",
      "92": "Сентябрь закат",
      "93": "Октябрь закат",
      "94": "Ноябрь закат",
      "95": "Декабрь закат",
      "96": "Резерв",
      "97": "Резерв",
      "98": "Резерв",
      "99": "Резерв",
      "100": "Панель команд",
      "101": "Панель команд 2",
      "102": "Резерв",
      "103": "Резерв",
      "104": "Резерв",
      "105": "Резерв",
      "106": "Резерв",
      "107": "Резерв",
      "108": "Резерв",
      "109": "Резерв",
      "110": "Время 1 кормления",
      "111": "Время 2 кормления",
      "112": "Время 3 кормления",
      "113": "Время 4 кормления",
      "114": "Время 5 кормления",
      "115": "Время 6 кормления",
      "116": "Время 7 кормления",
      "117": "Время 8 кормления",
      "118": "Время 9 кормления",
      "119": "Время 10 кормления",
      "120": "Время 11 кормления",
      "121": "Время 12 кормления",
      "122": "Время 13 кормления",
      "123": "Время 14 кормления",
      "124": "Время 15 кормления",
      "125": "Длительность 1 кормления",
      "126": "Длительность 2 кормления",
      "127": "Длительность 3 кормления",
      "128": "Длительность 4 кормления",
      "129": "Длительность 5 кормления",
      "130": "Длительность 6 кормления",
      "131": "Длительность 7 кормления",
      "132": "Длительность 8 кормления",
      "133": "Длительность 9 кормления",
      "134": "Длительность 10 кормления",
      "135": "Длительность 11 кормления",
      "136": "Длительность 12 кормления",
      "137": "Длительность 13 кормления",
      "138": "Длительность 14 кормления",
      "139": "Длительность 15 кормления",
      "140": "Резерв",
      "141": "Текущая минута последнего обновления",
      "142": "Текущий час последнего обновления",
      "143": "Число последнего обновления",
      "144": "Месяц последнего обновления",
      "145": "Год последнего обновления"
    };

    // Названия для битов регистров
    const bitNames = {
      "0": ["Охлаждение", "Задержка управления", "Загазованно (датчик)", "Темп. ниже нуля (улица)", "Темп. ниже нуля (курятник)", "Прием команд", "Передача включена", "Связь с удаленным модулем",
            "Вкл. движение вниз", "Вкл. движение вверх", "Основное освещение", "Дежурное освещение", "Приточный вентилятор", "Вытяжной вентилятор", "Нагрев", "Реле осушителя"],
      "1": ["Сон по расписанию", "На улице светло", "Дверь открыта", "Вкл. авто. проветривание", "Вкл. ручн. проветривание", "Аварийный режим нагревателя", "Коррекция вентиляторов", "Управление осушителем",
            "Датч. загаз. превышен (в час)", "Авария батарейки", "Авария датчика температуры", "Опрос датчика газа", "Опрос датчика двери", "Упр. вентиляторами днем", "Упр. вентиляторами во время сна", "Ручн. упр. вентиляцией"],
      "2": ["Упр. лазом по календарю", "Упр. закрытием лаза по расписанию", "Включен экран отображения вентиляции", "Включен экран отображения температур", "Упр. вентиляцией по ГОСТ", "Лаз используется", "Запрет открытия лаза по температуре", "Запрет открытия лаза по температуре на улице",
            "Лаз открыт", "Лаз в крайнем положении", "Ручн. режим упр. лазом", "Есть движение лаза", "Лаз зафиксирован", "Резерв", "Резерв", "Упр. лазом по температуре"],
      "3": ["Упр. светом использует календарь", "При отключённом календаре свет днём (0 — вкл., 1 — выкл.)", "Настройка по Wi-Fi", "Находимся в меню НАСТРОЙКА", "Находимся в одном из ручных режимов управления", "Ошибка при расчете вентиляции", "Нажата кнопка на приборе", "Управление охлаждением", 
            "Пропуски кормления", "Ошибки при кормлении", "-", "Кормление за сутки закончилось", "Количество кормлений = 0", "-", "Включен режим ручного кормления", "Отработка пропусков кормления"],
      "4": ["Подключена К1", "Подключена К2", "Подключена К3", "Подключена К4", "Используется осушитель", "-", "-", "-", 
            "Вкл. в сеть успройство К1", "Вкл. в сеть успройство К2", "Вкл. в сеть успройство К3", "Вкл. в сеть успройство К4", "Вкл. осушитель в сеть", "Вкл. в сеть новый модуль", "-", "-"],
      "100": ["Бит 0", "Бит 1", "Бит 2", "Бит 3", "Бит 4", "Бит 5", "Бит 6", "Бит 7", "Бит 8", "Бит 9", "Бит 10", "Бит 11", "Бит 12", "Бит 13", "Бит 14", "Бит 15"]
    };

    const commandOptions = [
      [0x0101, 'Включить проветривание'], [0x0102, 'Сбросить аварию датчика температуры'],
      [0x0103, 'Открыть лаз и зафиксировать'], [0x0104, 'Закрыть лаз и зафиксировать'],
      [0x0105, 'Зафиксировать лаз'], [0x0106, 'Расфиксировать лаз'],
      [0x0107, 'Сбросить ошибки датчика загазованности'],
      [0x0108, 'Выдать корм кормушкой К1'], [0x0109, 'Выдать корм кормушкой К2'],
      [0x010A, 'Выдать корм кормушкой К3'], [0x010B, 'Выдать корм кормушкой К4'],
      [0x01FF, 'Перезагрузить контроллер']
    ];
    async function sendCommand() {
      const message = document.getElementById('command-message');
      const select = document.getElementById('command-select');
      const value = select.value;
      if (!value) { message.textContent = 'Выберите команду'; return; }
      try {
        const response = await fetch('/api/write_reg', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:new URLSearchParams({addr:'100', value})});
        const result = await response.text();
        const errors = {register_busy:'Командный регистр занят', invalid_value:'Недопустимая команда', read_only:'Регистр доступен только для чтения', lock_busy:'Устройство занято, повторите запрос'};
        message.textContent = response.ok ? 'Команда принята ESP32' : (errors[result] || 'Не удалось отправить команду');
        if (response.ok && select.value === value) select.value = ''; 
        updateCommands(); updateData();
      } catch (_) { message.textContent = 'Ошибка связи с ESP32'; }
    }
    function updateCommands() {
      fetch('/api/commands', {cache:'no-store'}).then(r => { if (!r.ok) throw Error(); return r.json(); }).then(data => {
        const target = document.getElementById('command-state');
        if (!target) return;
        const c = data['100'];
        const labels = {idle:'Команды ещё не отправлялись', pending:'Ожидание выполнения команды', completed:'Выполнение подтверждено контроллером', unknown:'Состояние команды неизвестно'};
        target.textContent = c.unconfirmed ? 'Команда не подтверждена' : (labels[c.status] || 'Состояние команды неизвестно');
      }).catch(() => { const target = document.getElementById('command-state'); if (target) target.textContent = 'Не удалось получить состояние команды'; });
    }

    // BEGIN sunny register helpers
    function matchesRegisterSearch(address, title, names, query) {
      const q = String(query || '').trim().toLocaleLowerCase('ru').replace(/^r(?=\d)/, '');
      return !q || [String(address), String(title || ''), ...(names || [])].join(' ').toLocaleLowerCase('ru').includes(q);
    }
    function refreshFlagIndicators(nodes, raw) {
      const value = Number(raw) >>> 0;
      nodes.forEach(node => {
        const on = !!(value & (1 << Number(node.dataset.bit)));
        node.classList.toggle('active', on);
        node.querySelector('b').textContent = on ? '1' : '0';
      });
    }
    // END sunny register helpers
    function applyRegisterSearch() {
      const search = document.getElementById('register-search');
      const query = search ? search.value : '';
      let visible = 0;
      document.querySelectorAll('#tableBody tr').forEach(row => {
        const key = row.id.slice(4);
        const show = matchesRegisterSearch(key, regTitles[key] || '', bitNames[key] || [], query);
        row.hidden = !show;
        if (show) visible++;
      });
      document.getElementById('register-count').textContent = 'Показано регистров: ' + visible;
    }
    document.getElementById('register-search').addEventListener('input', applyRegisterSearch);
    applyRegisterSearch();
    function updateData() {
      fetch('/api/data').then(r => { if (!r.ok) throw Error(); return r.json(); }).then(data => {
        document.getElementById('register-error').textContent = '';
        for (const [key, value] of Object.entries(data)) {
          const row = document.getElementById('reg-' + key);
          if (!row) continue;
          const valCell = row.querySelector('.val-cell');
          const oldValue = row.getAttribute('data-last-val');
          if (oldValue !== null && oldValue !== String(value)) {
            row.classList.remove('flash-active'); void row.offsetWidth; row.classList.add('flash-active');
          }
          row.setAttribute('data-last-val', value);
          const title = regTitles[key] || `Регистр №${key}`;
          if (!valCell.hasAttribute('data-register-ready')) {
            if (key === '100') {
              const options = commandOptions.map(([code, label]) => `<option value="${code}">0x${code.toString(16).toUpperCase().padStart(4, '0')} — ${label}</option>`).join('');
              valCell.innerHTML = `<div class="register-summary"><div class="register-title">${title}</div><span class="register-value">Команды</span></div><details class="register-command"><summary>Команды контроллеру</summary><label for="command-select">Команда контроллеру</label><select id="command-select"><option value="">Выберите команду</option>${options}</select><button type="button" class="btn" onclick="sendCommand()">Отправить</button><div id="command-message" role="status"></div><div id="command-state" role="status"></div><div>Значение R100: <span id="command-value"></span></div></details>`;
              updateCommands();
            } else if (['0','1','2','3','4'].includes(key)) {
              const flags = bitNames[key].map((name, i) => `<div class="register-flag" data-bit="${i}"><span><small>B${i}</small> ${name === '-' ? 'Не обозначен' : name}</span><b>—</b></div>`).join('');
              valCell.innerHTML = `<div class="register-summary"><div class="register-title">${title}</div><div><span class="register-value">—</span></div></div><details><summary>Показать флаги · 16 бит</summary><div class="register-flags">${flags}</div></details>`;
            } else {
              valCell.innerHTML = `<div class="register-summary"><div class="register-title">${title}</div><span class="register-value">—</span></div>`;
            }
            valCell.setAttribute('data-register-ready', '1');
          }
          if (key === '100') {
            document.getElementById('command-value').textContent = `0x${Number(value).toString(16).toUpperCase().padStart(4, '0')}`;
          } else {
            valCell.querySelector('.register-value').textContent = String(value);
            if (['0','1','2','3','4'].includes(key)) refreshFlagIndicators(valCell.querySelectorAll('.register-flag'), value);
          }
        }
      }).catch(() => { document.getElementById('register-error').textContent = 'Не удалось получить данные регистров'; });
    }


    setInterval(updateData, 1000);
    setInterval(updateCommands, 1000);
    updateData();
)tableasset";
