#include "web_pages.h"

extern bool isCurrentRequestAdmin();

static String escapeHtml(const String& value) {
  String escaped;
  escaped.reserve(value.length());
  for (size_t i = 0; i < value.length(); ++i) {
    switch (value[i]) {
      case '&': escaped += "&amp;"; break;
      case '<': escaped += "&lt;"; break;
      case '>': escaped += "&gt;"; break;
      case '"': escaped += "&quot;"; break;
      case '\'': escaped += "&#39;"; break;
      default: escaped += value[i]; break;
    }
  }
  return escaped;
}

static String getDesignSystemStyles() {
  return "<style>"
         " :root { --brand:#2563eb; --brand-dark:#1d4ed8; --page:#f1f5f9; --surface:#ffffff; --ink:#1e293b; --muted:#64748b; --line:#e2e8f0; --success:#15803d; --danger:#b91c1c; --warning:#b45309; }"
         " body { background:var(--page); color:var(--ink); font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif; }"
         " h1,h2 { color:var(--ink); font-weight:700; text-align:center; margin:0 0 20px; } h1 { font-size:24px; } h2 { font-size:23px; line-height:1.3; }"
         " h3 { color:#334155; margin:0 0 15px; border-bottom:1px solid var(--line); padding-bottom:9px; font-size:15px; font-weight:700; letter-spacing:.04em; text-transform:uppercase; }"
         " .sub-title { margin:0 0 12px; padding-bottom:8px; border-bottom:1px solid var(--line); color:#334155; font-size:15px; font-weight:700; letter-spacing:.04em; text-transform:uppercase; }"
         " .card { box-sizing:border-box; padding:20px; margin-bottom:20px; background:var(--surface); border:1px solid var(--line); border-radius:14px; box-shadow:0 4px 16px rgba(15,23,42,.06); }"
         " .admin-mode-banner { width:100%; max-width:var(--content-max,600px); box-sizing:border-box; align-self:center; margin-left:auto !important; margin-right:auto !important; }"
         " .status-row { display:flex; justify-content:space-between; align-items:center; gap:12px; padding:11px 8px; border-bottom:1px solid #edf1f5; } .status-row:last-child { border-bottom:0; }"
         " .badge { display:inline-block; padding:5px 11px; border-radius:999px; font-size:13px; font-weight:700; line-height:1.25; min-width:64px; text-align:center; }"
         " .bg-success,.bg-on { background:var(--success); color:#fff; } .bg-danger,.bg-alarm { background:var(--danger); color:#fff; }"
         " .bg-info { background:var(--brand); color:#fff; } .bg-neutral,.bg-normal,.bg-off { background:#e2e8f0; color:#334155; }"
         " .val-box { padding:4px 12px; color:var(--ink); background:#f8fafc; border:1px solid var(--line); border-radius:8px; font-family:monospace; font-size:15px; font-weight:700; }"
         " .btn { min-height:44px; box-sizing:border-box; display:flex; align-items:center; justify-content:center; margin:10px 0; padding:11px 14px; border:0; border-radius:10px; background:var(--brand); color:#fff; font-family:inherit; font-size:15px; font-weight:600; line-height:1.35; cursor:pointer; text-decoration:none; }"
         " .btn-secondary { background:#475569; } .btn:hover,.nav-btn:hover { filter:brightness(.93); transform:translateY(-1px); }"
         " .nav-btn { min-height:48px; display:flex; align-items:center; justify-content:center; border-radius:12px; color:#fff; font-size:15px; font-weight:600; text-align:center; text-decoration:none; box-shadow:0 4px 12px rgba(15,23,42,.12); }"
         " .menu-btn { border:1px solid var(--line); border-radius:14px; box-shadow:0 4px 16px rgba(15,23,42,.06); }"
         " input:not([type=checkbox]):not([type=radio]):not([type=submit]):not([type=button]),select,textarea { box-sizing:border-box; width:100%; padding:12px; margin:8px 0; border:1px solid var(--line); border-radius:8px; background:var(--surface); color:var(--ink); font:16px/1.4 'Segoe UI',Tahoma,Geneva,Verdana,sans-serif; }"
         " table { width:100%; border-collapse:collapse; background:var(--surface); } th,td { padding:10px 12px; border:1px solid var(--line); text-align:center; } th { background:#f8fafc; color:#475569; font-size:13px; text-transform:uppercase; }"
         " :where(a,button,input,select,textarea,[tabindex]):focus-visible { outline:3px solid #ffbf47; outline-offset:3px; box-shadow:0 0 0 2px #1f2937; }"
         " .switch input:focus-visible + .slider { outline:3px solid #ffbf47; outline-offset:3px; }"
         " .field-label { display:block; margin:10px 0 2px; color:#374151; font-size:14px; font-weight:600; line-height:1.4; }"
         " .field-hint { margin:2px 0 6px; color:#555; font-size:14px; line-height:1.45; }"
         " @media (prefers-reduced-motion: reduce) { *, *::before, *::after { scroll-behavior:auto !important; animation-duration:0.01ms !important; animation-iteration-count:1 !important; transition-duration:0.01ms !important; } }"
         "</style>";
}

String getAdminModeBanner() {
  if (isCurrentRequestAdmin()) {
    return "<div class='admin-mode-banner' style='background:#fff1d6;border:2px solid #e9a23b;color:#713f12;padding:14px;border-radius:10px;margin:0 0 18px;text-align:center;font-weight:700;'>⚠️ Расширенный режим активен: запись и OTA разрешены.<form method='POST' action='/admin/disable' style='margin:8px 0 0'><button class='btn' style='background:#b42318;margin:0' type='submit'>🔒 Перейти в безопасный режим</button></form></div>";
  }
  return "";
}

// --- Вспомогательная функция для HTML обертки ---
String getHeader(String title) {
  return "<html><head><meta charset='UTF-8'>"
         "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
         "<style>"
         "  body { --content-max:500px; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }"
         "  .container { width: 100%; max-width: 500px; }"
         "</style>" + getDesignSystemStyles() + "<title>" + title + "</title></head><body>"
         "<div class='container'>" + getAdminModeBanner() +
         "<a href='/' class='btn btn-secondary' style='margin-bottom:20px;'>🏠 Главная</a>";
}

// 1. Главная страница
  void handleIndex() {
  //String html = getHeader("Панель управления");
  String html = "<html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<style>"
        "  body { --content-max:500px; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }"
        "  .container { width: 100%; max-width: 500px; }"
        "  .top-bar { display: flex; justify-content: space-between; align-items: center; width: 100%; max-width: 500px; box-sizing: border-box; }"
        "  .signals-wrapper { display: flex; align-items: center; gap: 15px; }"
        "</style>"
        + getDesignSystemStyles() +
        "<title>Панель управления</title></head>"
        "<body>";
  html += getAdminModeBanner();
  html += "<div class='top-bar card'>"
          // Левая часть (Время)
          "<div id='rtc-current-time' class='fw-bold fs-5'>🕒--:--:-- --.--.--</div>"
    
          // Правая часть (Сигналы вместе)
          "<div class='signals-wrapper'>"
            "<span class='sig-box' id='wifi-bars' title='Wi-Fi'>"
              "<div class='bar b1'></div>"
              "<div class='bar b2'></div>"
              "<div class='bar b3'></div>"
              "<div class='bar b4'></div>"
            "</span>"
            "<span id='mqtt-dot' class='dot' title='MQTT'></span>"
          "</div>"
        "</div>";
         

  // 1. Стили интерфейса
  html += R"rawliteral(
  <style>
    .status-card { margin-bottom: 20px; }
    .ip-info { font-size: 0.85em; color: #666; margin-top: 5px; line-height: 1.5; }
    .ip-info b { color: #333; font-family: monospace; }
    .sig-box { display: flex; align-items: flex-end; height: 17px; gap: 3px; margin-top: 5px; cursor: pointer; }
    .bar { width: 4px; background: #e0e0e0; border-radius: 1px; transition: 0.3s; }
    .b1 { height: 5px; } .b2 { height: 9px; } .b3 { height: 13px; } .b4 { height: 17px; }
    .green { background: #28a745 !important; }
    .yellow { background: #ffc107 !important; }
    .red { background: #dc3545 !important; }
    .dot { height: 14px; width: 14px; border-radius: 50%; display: inline-block; margin-right: 8px; background: #bbb; vertical-align: middle; cursor: pointer; }
    .online { background: #28a745; box-shadow: 0 0 8px rgba(40,167,69,0.4); }
    .offline { background: #dc3545; }
    .nav-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; margin-top: 20px; }
    .alert-banner { display: none; background: #fee2e2; border: 1px solid #fca5a5; color: #991b1b; padding: 10px; border-radius: 8px; text-align: center; font-weight: bold; margin-bottom: 15px; font-size: 14px; animation: pulse 2s infinite; }
    @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.8; } 100% { opacity: 1; } }
  
    /* Карточка температуры */
    .temp-card-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-top: 5px; }
    .temp-block { background: #f8fafc; border: 1px solid #eaecf4; border-radius: 8px; padding: 10px; text-align: center; }
    .temp-block .loc-title { font-size: 14px; color: #64748b; font-weight: 600; margin-bottom: 5px; text-transform: uppercase; }
    .temp-block .temp-val { font-size: 20px; font-weight: bold; color: #1e293b; }
    .temp-block .hum-val { font-size: 14px; color: #64748b; margin-top: 2px; }
    
    /* Карточка вентиляции */
    .txt-bold { font-weight: bold; color: #1e293b; }
    .status-highlight { font-weight: bold; padding: 2px 8px; border-radius: 6px; font-size: 14px; }
    .feed-missed-alert { display: none; background: #fee2e2; border: 1px solid #fca5a5; color: #991b1b; padding: 6px 12px; border-radius: 6px; font-weight: bold; font-size: 14px; text-align: center; margin-top: 10px; }
    </style>
  )rawliteral";

  html += "<div class='container'>";
  
  // Блок критических аварий (скрыт по умолчанию, появляется при ошибках)
  html += "<div id='alert-banner' class='alert-banner'>⚠️ ВНИМАНИЕ: Обнаружены активные ошибки или неисправности!</div>";

  // --- НОВАЯ КАРТОЧКА: ТЕМПЕРАТУРА И ВЛАЖНОСТЬ ---
  html += "<div class='card'>";
  html += "  <div class='sub-title'>🌡️ Температура и влажность</div>";
  html += "  <div class='temp-card-grid'>";
  html += "    <div class='temp-block'>";
  html += "      <div class='loc-title'>🏡 В курятнике</div>";
  html += "      <div class='temp-val' id='main-temp-in'>--°C</div>";
  html += "      <div class='hum-val'>Влажность: <span id='main-hum-in'>--</span>%</div>";
  html += "    </div>";
  html += "    <div class='temp-block'>";
  html += "      <div class='loc-title'>🌳 На улице</div>";
  html += "      <div class='temp-val' id='main-temp-out'>--°C</div>";
  html += "      <div class='hum-val'>Влажность: <span id='main-hum-out'>--</span>%</div>";
  html += "    </div>";
  html += "  </div>";
  html += "  <div class='status-row' style='margin-top: 12px; border-bottom: none; padding-bottom: 0;'>";
  html += "    <span>Нагреватель:</span>";
  html += "    <span id='heater-status' class='txt-bold'>--</span>";
  html += "  </div>";
  html += "  <div class='status-row' id='row-dehumidifier' style='display: none; border-bottom: none; padding-bottom: 0;'>";
  html += "    <span>Осушитель:</span>";
  html += "    <span id='dehumidifier-status' class='txt-bold'>--</span>";
  html += "  </div>";
  html += "</div>";

  // --- ВЕНТИЛЯЦИЯ ---
  html += "<div class='card'>";
  html += "  <div class='sub-title' style='display: flex; justify-content: space-between; align-items: center;'>";
  html += "    <span>💨 Вентиляция</span>";
  html += "    <span id='vent-disabled-label' style='color: #dc3545; font-weight: bold; font-size: 14px; display: none;'>ОТКЛЮЧЕНО</span>";
  html += "  </div>";
  html += "  <div id='vent-card-content'>";
  html += "    <div class='status-row'><span>Вентиляторы:</span><span id='vent-fans-count' class='txt-bold'>--</span></div>";
  html += "    <div class='status-row' id='row-vent-in'><span>Приточный вентилятор:</span><span id='vent-in-status' class='txt-bold'>--</span></div>";
  html += "    <div class='status-row' id='row-vent-out'><span>Вытяжной вентилятор:</span><span id='vent-out-status' class='txt-bold'>--</span></div>";
  html += "    <div class='status-row'><span>Режим управления:</span><span id='vent-mode-name' class='txt-bold'>--</span></div>";
  html += "    <div class='status-row'><span>Параметры работы:</span><span id='vent-auto-params' style='color:#555;'>Вкл/час: <b id='vent-r34'>--</b> | Длит: <b id='vent-r35'>--</b> мин</span></div>";
  html += "    <div class='status-row' style='border-bottom:none; padding-bottom:0;'><span>Состояние системы:</span><span id='vent-priority-status' class='status-highlight'>--</span></div>";
  html += "  </div>";
  html += "</div>";

  // --- ЛАЗ ---
  html += "<div class='card'>";
  html += "  <div class='sub-title' style='display: flex; justify-content: space-between; align-items: center;'>";
  html += "    <span>🐓 Лаз</span>";
  html += "    <span id='door-disabled-label' style='color: #dc3545; font-weight: bold; font-size: 14px; display: none;'>ОТКЛЮЧЕНО</span>";
  html += "  </div>";
  html += "  <div id='door-card-content'>";
  html += "    <div class='status-row'><span>Режим управления:</span><span id='door-priority-mode' class='txt-bold'>--</span></div>";
  html += "    <div class='status-row' style='border-bottom:none; padding-bottom:0;'><span>Состояние системы:</span><span id='door-complex-status' style='text-align:right; font-weight:600; max-width:65%; font-size:14px;'>--</span></div>";
  html += "  </div>";
  html += "</div>";

  // --- КОРМЛЕНИЕ ---
  html += "<div class='card'>";
  html += "  <div class='sub-title' style='display: flex; justify-content: space-between; align-items: center;'>";
  html += "    <span>🌾 Кормление</span>";
  html += "    <span id='feed-disabled-label' style='color: #dc3545; font-weight: bold; font-size: 14px; display: none;'>ОТКЛЮЧЕНО</span>";
  html += "  </div>";
  html += "  <div id='feed-card-content'>";
  html += "    <div class='status-row'><span>Режим управления:</span><span id='feed-control-mode' class='txt-bold'>--</span></div>";
  html += "    <div class='status-row'><span>Функция отработки пропусков кормления:</span><span id='feed-missed-feature' class='txt-bold'>--</span></div>";
  html += "    <div class='status-row'><span>Режим работы:</span><span id='feed-operation-mode' style='text-align:right; font-weight:500; font-size:14px; max-width:65%;'>--</span></div>";
  html += "    <div class='status-row' style='border-bottom:none; padding-bottom:0;'><span>Кормление за сутки:</span><span>Выполнено <b id='feed-done-count' class='txt-bold'>--</b> из <b id='feed-plan-count' class='txt-bold'>--</b></span></div>";
  html += "    <div id='feed-missed-warning' class='feed-missed-alert'>⚠️ Выполняется пропуск кормления!</div>";
  html += "  </div>";
  html += "</div>";

  // --- ОСВЕЩЕНИЕ ---
  html += "<div class='card'>";
  html += "  <div class='sub-title'>💡 Освещение</div>";
  html += "  <div class='status-row'><span>Режим управления:</span><span id='light-control-mode' class='txt-bold'>--</span></div>";
  html += "  <div class='status-row'><span>Основной свет:</span><span id='light-main-status' class='txt-bold'>--</span></div>";
  html += "  <div class='status-row' style='border-bottom:none; padding-bottom:0;'><span>Дежурный свет:</span><span id='light-aux-status' class='txt-bold'>--</span></div>";
  html += "</div>";

  // Навигационная сетка кнопок
  html += "<div class='nav-grid'>";
  html += "  <a href='/autoChickenHous' class='nav-btn' style='background: #d97706; grid-column: span 2;'>🎛 Автоматизация курятника</a>";
  html += "  <a href='/table' class='nav-btn' style='background: #4e73df;'>📊 Все регистры</a>";
  html += "  <a href='/settings' class='nav-btn' style='background: #1cc88a;'>⚙️ Настройки</a>";
  html += "  <a href='/update' class='nav-btn' style='background: #64748b; grid-column: span 2;' "
          "onclick=\"return confirm('Перейти на страницу обновления прошивки? Мониторинг автоматики будет временно приостановлен.');\">"
          "🔄 Обновление ПО (OTA)</a>";
  html += "</div>";

  // Скрипт динамического обновления
  html += R"rawliteral(
  <script>
  // Утилита для перевода минут из регистров в ЧЧ:ММ
  function parseMinutesToTime(totalMinutes) {
    if (totalMinutes === undefined || totalMinutes === null || isNaN(totalMinutes)) return "--:--";
    const mins = parseInt(totalMinutes);
    if (mins === 0) return "--:--";
    const hours = Math.floor(mins / 60);
    const minutes = mins % 60;
    return `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}`;
  }
  // Утилита для перевода десятых долей секунды из регистра в формат С.Ц (например, 15 -> 1.5)
  function parseDeciSeconds(val) {
    if (val === undefined || val === null || isNaN(val)) return "--";
    const num = parseInt(val);
    return (num / 10).toFixed(1);
  }
  function update() {
    // 1. Запрос системного статуса (WiFi и MQTT)
    fetch('/api/status').then(r => r.json()).then(d => {
      const bars = document.querySelectorAll('.top-bar .bar');
      const dot = document.querySelector('.top-bar .dot');
      
      if (d.wifi_conn) {
        let count = 0, colorClass = '';
        if (d.rssi >= -60) { count = 4; colorClass = 'green'; }
        else if (d.rssi >= -75) { count = 3; colorClass = 'green'; }
        else if (d.rssi >= -85) { count = 2; colorClass = 'yellow'; }
        else { count = 1; colorClass = 'red'; }
        
        bars.forEach((b, i) => {
          b.classList.remove('green', 'yellow', 'red');
          if (i < count) b.classList.add(colorClass);
        });
      } else {
        bars.forEach(b => b.classList.remove('green', 'yellow', 'red'));
      }

      if (dot) {
        dot.className = d.mqtt_conn ? 'dot online' : 'dot offline';
      }
    }).catch(err => console.error("Ошибка API статуса:", err));

    // 2. Запрос Modbus данных для сводной панели
    fetch('/api/data').then(r => r.json()).then(data => {

      //Сборка текущего системного времени из регистров 17-22
      if (data["19"] !== undefined && data["18"] !== undefined && data["17"] !== undefined) {
        const hh = String(data["19"]).padStart(2, '0'); // Часы
        const mm = String(data["18"]).padStart(2, '0'); // Минуты
        const ss = String(data["17"]).padStart(2, '0'); // Секунды
        
        const day = String(data["20"] || 0).padStart(2, '0'); // День
        const month = String(data["21"] || 0).padStart(2, '0'); // Месяц
        const year = String(data["22"] || 0).slice(-2).padStart(2, '0'); // Две последние цифры года
        
        document.getElementById('rtc-current-time').innerText = `🕒${hh}:${mm}:${ss} ${day}.${month}.${year}`;
      } else {
        document.getElementById('rtc-current-time').innerText = "🕒--:--:-- --.--.--";
      }

      // Парсинг битовых регистров масок
      const r0 = data["0"] || 0;
      const r1 = data["1"] || 0;
      const r2 = data["2"] || 0;
      const r3 = data["3"] || 0;
      const r4 = data["4"] || 0;

      // -- ЛОГИКА ДЛЯ КАРТОЧКИ ТЕМПЕРАТУРЫ (С учётом знаковых битов) --
      // Курятник: температура в R5, влажность в R6, знак в R0 бите 4
      if (data["5"] !== undefined) {
        let tIn = parseInt(data["5"]);
        const signIn = (r0 >> 4) & 1; // 4-й бит регистра 0 отвечает за минус в курятнике
        if (signIn === 1) tIn = tIn * -1;
        document.getElementById('main-temp-in').innerText = tIn + "°C";
      }
      if (data["6"] !== undefined) {
        document.getElementById('main-hum-in').innerText = data["6"];
      }

      // Улица: температура в R7, влажность в R8, знак в R0 бите 3
      if (data["7"] !== undefined) {
        let tOut = parseInt(data["7"]);
        const signOut = (r0 >> 3) & 1; // 3-й бит регистра 0 отвечает за минус на улице
        if (signOut === 1) tOut = tOut * -1;
        document.getElementById('main-temp-out').innerText = tOut + "°C";
      }
      if (data["8"] !== undefined) {
        document.getElementById('main-hum-out').innerText = data["8"];
      }

      // ЛОГИКА ДЛЯ НАГРЕВАТЕЛЯ
      const isHeaterOn = (r0 >> 14) & 1;       // Рег 0 Бит 14
      const isEmergency1 = (r1 >> 10) & 1;     // Рег 1 Бит 10
      const isEmergency2 = (r1 >> 5) & 1;      // Рег 1 Бит 5

      const heaterEl = document.getElementById('heater-status');
      if (heaterEl) {
        if (isEmergency1 === 1 && isEmergency2 === 1) {
          heaterEl.innerHTML = "<span style='color:#dc3545; font-weight:bold;'>Аварийное управление ⚠️</span>";
        } else if (isHeaterOn === 1) {
          heaterEl.innerHTML = "<span style='color:#16a34a;'>Включен 🟢</span>";
        } else {
          heaterEl.innerHTML = "<span style='color:#64748b;'>Выключен ⚪</span>";
        }
      }

      // ЛОГИКА ДЛЯ ОСУШИТЕЛЯ
      const isDehumidifierUsed = (r4 >> 12) & 1; // Рег 4 Бит 12 (Используется ли осушитель)
      const isDehumidifierOn = (r0 >> 15) & 1;   // Рег 0 Бит 15 (Включен/Выключен)

      const rowDehumidifier = document.getElementById('row-dehumidifier');
      const dehumidifierStatusEl = document.getElementById('dehumidifier-status');

      if (isDehumidifierUsed === 1) {
        // Если осушитель используется, показываем всю строку
        if (rowDehumidifier) rowDehumidifier.style.display = "flex";
        
        // Меняем статус Вкл/Выкл
        if (dehumidifierStatusEl) {
          if (isDehumidifierOn === 1) {
            dehumidifierStatusEl.innerHTML = "<span style='color:#16a34a;'>Включен 🟢</span>";
          } else {
            dehumidifierStatusEl.innerHTML = "<span style='color:#64748b;'>Выключен ⚪</span>";
          }
        }
      } else {
        // Если осушитель не используется в системе, полностью скрываем строку с экрана
        if (rowDehumidifier) rowDehumidifier.style.display = "none";
      }

      // -- ЛОГИКА ДЛЯ КАРТОЧКИ ВЕНТИЛЯЦИИ --
      const fansCount = data["53"] !== undefined ? parseInt(data["53"]) : 0;
      const ventDisabledLabel = document.getElementById('vent-disabled-label');
      const ventCardContent = document.getElementById('vent-card-content');

      if (fansCount === 0) {
        // Если вентиляторов 0, показываем надпись "ОТКЛЮЧЕНО" и скрываем контент карточки
        if (ventDisabledLabel) ventDisabledLabel.style.display = "inline";
        if (ventCardContent) ventCardContent.style.display = "none";
      } else {
        // Если вентиляторы используются, возвращаем обычный вид
        if (ventDisabledLabel) ventDisabledLabel.style.display = "none";
        if (ventCardContent) ventCardContent.style.display = "block";

        const ventFansCountEl = document.getElementById('vent-fans-count');
        if (ventFansCountEl) ventFansCountEl.innerHTML = `${fansCount} шт.`;
        
        // Проверка битов включения (R0:B12 и R0:B13)
        const isVentInOn = (r0 >> 12) & 1;
        const isVentOutOn = (r0 >> 13) & 1;

        const ventInStatusEl = document.getElementById('vent-in-status');
        const ventOutStatusEl = document.getElementById('vent-out-status');
        if (ventInStatusEl) ventInStatusEl.innerHTML = isVentInOn === 1 ? "<span style='color:#16a34a;'>Включен 🟢</span>" : "<span style='color:#64748b;'>Выключен ⚪</span>";
        if (ventOutStatusEl) ventOutStatusEl.innerHTML = isVentOutOn === 1 ? "<span style='color:#16a34a;'>Включен 🟢</span>" : "<span style='color:#64748b;'>Выключен ⚪</span>";

        // Режим работы (Регистр 1 Бит 15) и уставки (Регистры 34, 35)
        const isManualMode = (r1 >> 15) & 1;
        const ventModeNameEl = document.getElementById('vent-mode-name');
        const ventR34El = document.getElementById('vent-r34');
        const ventR35El = document.getElementById('vent-r35');

        if (isManualMode === 1) {
          if (ventModeNameEl) ventModeNameEl.innerHTML = "<span style='color:#d97706;'>Ручной 🛠️</span>";
          if (ventR34El) ventR34El.innerText = "--";
          if (ventR35El) ventR35El.innerText = "--";
        } else {
          if (ventModeNameEl) ventModeNameEl.innerHTML = "<span style='color:#2563eb;'>Автоматический 🤖</span>";
          if (ventR34El) ventR34El.innerText = data["34"] !== undefined ? data["34"] : "--";
          if (ventR35El) ventR35El.innerText = data["35"] !== undefined ? data["35"] : "--";
        }

        // Расчет статуса по строгому приоритету
        const isGasAlarm = (r0 >> 2) & 1;      // Рег 0 Бит 2
        const isCoolingActive = (r0 >> 0) & 1;  // Рег 0 Бит 0
        const isAutoVent = (r1 >> 3) & 1;       // Рег 1 Бит 3
        const isManualVent = (r1 >> 4) & 1;     // Рег 1 Бит 4

        let priorityStatusText = "Пауза 💤";
        let statusColor = "#64748b"; 
        let statusBg = "#f1f5f9";

        if (isGasAlarm === 1) {
          priorityStatusText = "Загазованно ⚠️";
          statusColor = "#b91c1c"; 
          statusBg = "#fee2e2";
        } else if (isCoolingActive === 1) {
          priorityStatusText = "Охлаждение ❄️";
          statusColor = "#1d4ed8"; 
          statusBg = "#dbeafe";
        } else if (isManualVent === 1) {
          priorityStatusText = "Проветривание (Ручное) 💨";
          statusColor = "#b45309"; 
          statusBg = "#fef3c7";
        } else if (isAutoVent === 1) {
          priorityStatusText = "Проветривание (Авто) 🔄";
          statusColor = "#047857"; 
          statusBg = "#d1fae5";
        }

        const statusEl = document.getElementById('vent-priority-status');
        if (statusEl) {
          statusEl.innerText = priorityStatusText;
          statusEl.style.color = statusColor;
          statusEl.style.backgroundColor = statusBg;
        }
      }

      // --- ЛОГИКА ДЛЯ КАРТОЧКИ ЛАЗА ---
      const isDoorEnabled = (r2 >> 5) & 1; // Проверка 5-го бита регистра 2
      
      const doorDisabledLabel = document.getElementById('door-disabled-label');
      const doorCardContent = document.getElementById('door-card-content');

      if (isDoorEnabled === 0) {
        // Если бит равен 0, показываем надпись "ОТКЛЮЧЕНО" и скрываем контент карточки
        if (doorDisabledLabel) doorDisabledLabel.style.display = "inline";
        if (doorCardContent) doorCardContent.style.display = "none";
      } else {
        // Если бит равен 1, скрываем надпись и показываем карточку как обычно
        if (doorDisabledLabel) doorDisabledLabel.style.display = "none";
        if (doorCardContent) doorCardContent.style.display = "block";

        // 1. Вывод режима по старшинству приоритетов
        let doorModeText = "- 🤖";
        let doorModeColor = "#2563eb";

        if (((r2 >> 10) & 1) === 1) {
          doorModeText = "Ручной режим 🛠️";
          doorModeColor = "#d97706";
        } else if (((r2 >> 15) & 1) === 1) {
          doorModeText = "Авто по температуре 🌡️";
          doorModeColor = "#0284c7";
        } else if (((r2 >> 1) & 1) === 1) {
          doorModeText = "Закр. по расп. / Откр. по календ. 📅⏰";
          doorModeColor = "#7c3aed";
        } else if (((r2 >> 0) & 1) === 1) {
          doorModeText = "Авто по календарю 📅";
          doorModeColor = "#059669";
        }
        
        const dmEl = document.getElementById('door-priority-mode');
        if (dmEl) {
          dmEl.innerText = doorModeText;
          dmEl.style.color = doorModeColor;
        }

        // 2. Вывод состояния по алгоритму ветвления
        let doorStateText = "Неизвестно";
        let doorStateColor = "#475569";

        const isMoving = (r2 >> 11) & 1;
        const isGoingDown = (r0 >> 8) & 1;
        const isGoingUp = (r0 >> 9) & 1;
        const isBlocked = (r2 >> 12) & 1;
        const isExtremePosition = (r2 >> 9) & 1;
        const openCloseCheck = (r2 >> 8) & 1;
        const insideTempLock = (r2 >> 6) & 1;
        const outsideTempLock = (r2 >> 7) & 1;

        if (isMoving === 1) {
          if (isGoingDown === 1) {
            doorStateText = "⏳ Закрывается... ⬇️";
            doorStateColor = "#ea580c";
          } else if (isGoingUp === 1) {
            doorStateText = "⏳ Открывается... ⬆️";
            doorStateColor = "#059669";
          } else {
            doorStateText = "⏳ В движении";
            doorStateColor = "#d97706";
          }
        } else {
          if (isBlocked === 1) {
            if (isExtremePosition === 0) {
              doorStateText = "🛑 ЛАЗ ЗАФИКСИРОВАН В ПРОМЕЖУТОЧНОМ ПОЛОЖЕНИИ!";
            } else if (openCloseCheck === 1) {
              doorStateText = "🛑 ЛАЗ ЗАБЛОКИРОВАН ОТКРЫТЫМ!";
            } else {
              doorStateText = "🛑 ЛАЗ ЗАБЛОКИРОВАН ЗАКРЫТЫМ!";
            }
            doorStateColor = "#dc2626";
          } else if (isExtremePosition === 0) {
            doorStateText = "🔄 Промежуточное положение";
            doorStateColor = "#475569";
          } else {
            if (openCloseCheck === 1) {
              const closeTimeStr = parseMinutesToTime(data["9"]);
              doorStateText = `🟢 Открыт (Закрытие в ${closeTimeStr})`;
              doorStateColor = "#16a34a";
            } else if (openCloseCheck === 0) {
              if (insideTempLock === 1) {
                doorStateText = "❄️ Закрыт (Запрет открытия по темп. внутри)";
                doorStateColor = "#dc2626";
              } else if (outsideTempLock === 1) {
                doorStateText = "🥶 Закрыт (Запрет открытия по темп. на улице)";
                doorStateColor = "#dc2626";
              } else {
                const openTimeStr = parseMinutesToTime(data["10"]);
                doorStateText = `🔒 Закрыт (Открытие в ${openTimeStr})`;
                doorStateColor = "#2563eb";
              }
            }
          }
        }

        const dsEl = document.getElementById('door-complex-status');
        if (dsEl) {
          dsEl.innerText = doorStateText;
          dsEl.style.color = doorStateColor;
        }
      }

      // --- ЛОГИКА ДЛЯ КАРТОЧКИ КОРМЛЕНИЯ ---
      const isFeedEnabled = (r0 >> 7) & 1; // Регистр 0 Бит 7
      const feedDisabledLabel = document.getElementById('feed-disabled-label');
      const feedCardContent = document.getElementById('feed-card-content');

      if (isFeedEnabled === 0) {
        // Если кормление выключено (бит равен 0), пишем ОТКЛЮЧЕНО и прячем параметры
        if (feedDisabledLabel) feedDisabledLabel.style.display = "inline";
        if (feedCardContent) feedCardContent.style.display = "none";
      } else {
        // Если включено, скрываем надпись и обновляем параметры как обычно
        if (feedDisabledLabel) feedDisabledLabel.style.display = "none";
        if (feedCardContent) feedCardContent.style.display = "block";

        // Режим управления кормлением (Регистр 3 Бит 14)
        const isFeedManual = (r3 >> 14) & 1;
        const feedControlModeEl = document.getElementById('feed-control-mode');
        if (feedControlModeEl) {
          feedControlModeEl.innerHTML = isFeedManual === 1 ? "<span style='color:#d97706;'>Ручной 🛠️</span>" : "<span style='color:#2563eb;'>Автоматический 🤖</span>";
        }

        const isMissedFeatureEnabled = (r3 >> 15) & 1;
        const feedMissedFeatureEl = document.getElementById('feed-missed-feature');
        if (feedMissedFeatureEl) {
          feedMissedFeatureEl.innerHTML = isMissedFeatureEnabled === 1 
            ? "<span style='color:#16a34a;'>Вкл</span>" 
            : "<span style='color:#64748b;'>Выкл</span>";
        }

        const feedOperationModeEl = document.getElementById('feed-operation-mode');
        if (feedOperationModeEl) {
          const nextFeederNum = data["12"] !== undefined ? data["12"] : "--";
          const nextFeedTimeStr = parseMinutesToTime(data["16"]);
          const nextFeedDuration = parseDeciSeconds(data["13"]);
          feedOperationModeEl.innerHTML = `Кормушка №<b>${nextFeederNum}</b> в <b>${nextFeedTimeStr}</b><br><span style='color:#64748b; font-size:14px;'>(Длительность: ${nextFeedDuration} сек)</span>`;
        }

        // Текущее состояние счетчиков за сутки (Регистры 14 и 15)
        const feedDoneCountEl = document.getElementById('feed-done-count');
        const feedPlanCountEl = document.getElementById('feed-plan-count');
        if (feedDoneCountEl) feedDoneCountEl.innerText = data["14"] !== undefined ? data["14"] : "--";
        if (feedPlanCountEl) feedPlanCountEl.innerText = data["15"] !== undefined ? data["15"] : "--";

        // Проверка наличия пропуска кормления (Регистр 3 Бит 8)
        const hasFeedMiss = (r3 >> 8) & 1;
        const feedMissedWarningEl = document.getElementById('feed-missed-warning');
        
        if (feedMissedWarningEl) {
          if (isMissedFeatureEnabled === 1 && hasFeedMiss === 1) {
            feedMissedWarningEl.style.display = "block";
          } else {
            feedMissedWarningEl.style.display = "none";
          }
        }
      }

      // ЛОГИКА КАРТОЧКИ ОСВЕЩЕНИЯ
      const lightBit0 = (r3 >> 0) & 1; // Рег 3 Бит 0
      const lightBit1 = (r3 >> 1) & 1; // Рег 3 Бит 1
      const isMainLightOn = (r0 >> 10) & 1; // Рег 0 Бит 10
      const isAuxLightOn = (r0 >> 11) & 1;  // Рег 0 Бит 11

      // 1. Определение режима управления освещением
      const lightModeEl = document.getElementById('light-control-mode');
      if (lightModeEl) {
        if (lightBit0 === 1) {
          lightModeEl.innerHTML = "<span style='color:#059669;'>По календарю 📅</span>";
        } else if (lightBit0 === 0 && lightBit1 === 1) {
          lightModeEl.innerHTML = "<span style='color:#2563eb;'>По расписанию (свет включен) ⏰🟢</span>";
        } else if (lightBit0 === 0 && lightBit1 === 0) {
          lightModeEl.innerHTML = "<span style='color:#64748b;'>По расписанию (свет отключен) ⏰🛑</span>";
        } else {
          lightModeEl.innerText = "--";
        }
      }

      // 2. Статус основного света
      const mainLightEl = document.getElementById('light-main-status');
      if (mainLightEl) {
        if (isMainLightOn === 1) {
          mainLightEl.innerHTML = "<span style='color:#16a34a;'>Включен 💡🟢</span>";
        } else {
          mainLightEl.innerHTML = "<span style='color:#64748b;'>Выключен ⚪</span>";
        }
      }

      // 3. Статус дежурного (вспомогательного) света
      const auxLightEl = document.getElementById('light-aux-status');
      if (auxLightEl) {
        if (isAuxLightOn === 1) {
          auxLightEl.innerHTML = "<span style='color:#d97706;'>Включен 🌙🟡</span>";
        } else {
          auxLightEl.innerHTML = "<span style='color:#64748b;'>Выключен ⚪</span>";
        }
      }

      // АНАЛИЗ АВАРИЙ И ОШИБОК
      const batteryAlarm = (r1 >> 9) & 1;
      const sensorAlarm = (r1 >> 10) & 1;
      const calcAlarm = (r3 >> 5) & 1;
      const feedAlarm = (r3 >> 9) & 1;
      const feederErrors = (data["48"] || 0) + (data["49"] || 0) + (data["50"] || 0) + (data["51"] || 0) + (data["52"] || 0);
      const hasAnyError = batteryAlarm || sensorAlarm || calcAlarm || feedAlarm || feederErrors > 0;
      document.getElementById('alert-banner').style.display = hasAnyError ? "block" : "none";

    }).catch(err => console.error("Ошибка API данных:", err));
  }
  
  setInterval(update, 1000);
  update();
  </script>
  )rawliteral";

  html += "</div></body></html>";
  webServer.send(200, "text/html", html);
}

// 2. Эндпоинт для отдачи данных в формате JSON
void handleApiData() {
  std::map<uint16_t, uint16_t> snapshot;
  if (!snapshotRegisters(snapshot)) {
    webServer.send(503, "text/plain", "Modbus Busy");
    return;
  }
  JsonDocument doc;
  for (const auto& item : snapshot) doc[String(item.first)] = item.second;
  String json;
  serializeJson(doc, json);
  webServer.send(200, "application/json", json);
}

// 3. Страница с таблицей
void handleTable() {
    std::map<uint16_t, uint16_t> snapshot;
    if (!snapshotRegisters(snapshot)) {
      webServer.send(503, "text/plain", "Modbus Busy");
      return;
    }
    String html = getHeader("Мониторинг регистров");
    html += "<h2>Данные в реальном времени</h2>";
    
    // 1. Добавляем стили для индикаторов битов
    html += R"rawliteral(
    <style>
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
    </style>
    )rawliteral";

    html += "<table border='1' cellpadding='8' id='regTable'>";
    html += "<thead><tr><th>Регистр</th><th>Значение и флаги</th></tr></thead>";
    html += "<tbody id='tableBody'>";
    
    // Первоначальный вывод строк из памяти memo
    for (const auto& item : snapshot) {
        html += "<tr id='reg-" + String(item.first) + "'>";
        html += "<td><b>" + String(item.first) + "</b></td>";
        html += "<td class='val-cell'>" + String(item.second) + "</td></tr>";
    }
    html += "</tbody></table>";

    html += R"rawliteral(
    <script>

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
      "47": "Среднее время нагревателя за час в сутки",
      "48": "Количество ошибок по кормушке К1",
      "49": "Количество ошибок по кормушке К2",
      "50": "Количество ошибок по кормушке К3",
      "51": "Количество ошибок по кормушке К4",
      "52": "Ошибки от модуля Wi-Fi",
      "53": "Количество используемых вентиляторов",
      "54": "Производительность вентиляторов",
      "55": "Объем помещения",
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
            "Вкл. движение вниз", "Вкл. движение вверх", "Основное освещение", "Дежурное освещение", "Приточный вентилятор", "Вытяжной вентилятор", "Нагрев", "-"],
      "1": ["Сон по расписанию", "На улице светло", "Дверь открыта", "Вкл. авто. проветривание", "Вкл. ручн. проветривание", "-", "Коррекция вентиляторов", "Управление осушителем", 
            "Датч. загаз. превышен (в час)", "Авария батарейки", "Авария датчика температуры", "Опрос датчика газа", "Опрос датчика двери", "Упр. вентиляторами днем", "Упр. вентиляторами во время сна", "Ручн. упр. вентиляцией"],
      "2": ["Упр. лазом по календарю", "Упр. закрытием лаза по расписанию", "Включен экран отображения вентиляции", "Включен экран отображения температур", "Упр. вентиляцией по ГОСТ", "-", "Запрет открытия лаза по температуре", "Запрет открытия лаза по температуре на улице", 
            "Лаз открыт", "Лаз закрыт", "Ручн. режим упр. лазом", "Есть движение лаза", "Лаз зафиксирован", "Занято направлением движения лаза", "Занято направлением движения", "Упр. лазом по температуре"],
      "3": ["Упр. светом использует календарь", "При окткл. календаре свет горит постоянно", "Настройка по Wi-Fi", "Находимся в меню НАСТРОЙКА", "Находимся в одном из ручных режимов управления", "Ошибка при расчете вентиляции", "Нажата кнопка на приборе", "Управление охлаждением", 
            "Пропуски кормления", "Ошибки при кормлении", "-", "Кормление за сутки закончилось", "Количество кормлений = 0", "-", "Включен режим ручного кормления", "Отработка пропусков кормления"],
      "4": ["Подключена К1", "Подключена К2", "Подключена К3", "Подключена К4", "Используется осушитель", "-", "-", "-", 
            "Вкл. в сеть успройство К1", "Вкл. в сеть успройство К2", "Вкл. в сеть успройство К3", "Вкл. в сеть успройство К4", "Вкл. осушитель в сеть", "Вкл. в сеть новый модуль", "-", "-"],
      "100": ["Бит 0", "Бит 1", "Бит 2", "Бит 3", "Бит 4", "Бит 5", "Бит 6", "Бит 7", "Бит 8", "Бит 9", "Бит 10", "Бит 11", "Бит 12", "Бит 13", "Бит 14", "Бит 15"]
    };

    function sendBit(addr, bit, state) {
      // Отправляем команду и СРАЗУ вызываем обновление данных после ответа сервера
      fetch('/api/write_bit', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: new URLSearchParams({ addr, bit, state: state ? 1 : 0 }) })
      .then(r => { if (r.status === 401) { window.location.href = '/admin?next=' + encodeURIComponent(window.location.pathname); return; } if(r.ok) setTimeout(updateData, 100); });
    }

    function sendValue(addr, val) {
      // Проверка диапазона перед отправкой
      if (val < 0 || val > 255) {
          alert("Значение должно быть от 0 до 255");
          return;
      }
      fetch('/api/write_reg', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: new URLSearchParams({ addr, value: val }) })
          .then(r => { if (r.status === 401) { window.location.href = '/admin?next=' + encodeURIComponent(window.location.pathname); return; } if(r.ok) setTimeout(updateData, 100); });
    }

    function updateData() {
      fetch('/api/data').then(r => r.json()).then(data => {
        for (const [key, value] of Object.entries(data)) {
          let row = document.getElementById('reg-' + key);
          if (!row) continue;
          const valCell = row.querySelector('.val-cell');

          // 1. Логика анимации вспышки для ВСЕХ регистров при изменении
          const oldValue = row.getAttribute('data-last-val');
          if (oldValue !== null && oldValue !== String(value)) {
            row.classList.remove('flash-active');
            void row.offsetWidth; // Сброс для перезапуска анимации
            row.classList.add('flash-active');
          }
          row.setAttribute('data-last-val', value);

          // 2. Формируем заголовок
          const titleText = regTitles[key] ? regTitles[key] : `Регистр №${key}`;
          const headerHtml = `<div style="background:#4e73df; color:white; padding:5px; margin-bottom:10px; border-radius:4px; font-size:14px; text-align:center; font-weight:bold; text-transform:uppercase;">${titleText}</div>`;

          // --- КЕЙС 100 (ПАНЕЛЬ УПРАВЛЕНИЯ - ПЕРЕКЛЮЧАТЕЛИ) ---
          if (key == "100") {
            let html = headerHtml + `<div style="display:grid; grid-template-columns: 1fr 1fr; gap:8px; padding:4px;">`;
            for (let i = 0; i < 16; i++) {
              const isSet = (value >> i) & 1;
              const name = bitNames[key] ? bitNames[key][i] : `Бит ${i}`;
              html += `
                <div style="display:flex; align-items:center; justify-content:space-between; background:#f8f9fc; padding:5px 8px; border-radius:6px; border:1px solid #eaecf4;">
                  <span style="font-size:14px;">${name}</span>
                  <label class="switch"><input type="checkbox" aria-label="${name}" ${isSet ? 'checked' : ''} onchange="sendBit(${key}, ${i}, this.checked)"><span class="slider"></span></label>
                </div>`;
            }
            valCell.innerHTML = html + `</div>`;

          // --- КЕЙС 0-4 (ТОЛЬКО ОТОБРАЖЕНИЕ БИТОВ - ЦЕНТРИРОВАНИЕ + ФОН) ---
          } else if (['0', '1', '2', '3', '4'].includes(key)) {
            let html = headerHtml + `<div style="display:grid; grid-template-columns: 1fr 1fr; gap:6px; padding:4px;">`;
            for (let i = 0; i < 16; i++) {
              const isActive = (value >> i) & 1;
              const name = (bitNames[key] && bitNames[key][i]) ? bitNames[key][i] : `Бит ${i}`;
              const bgColor = isActive ? '#28a745' : '#f0f2f5';
              const textColor = isActive ? '#fff' : '#555';
              html += `
                <div style="display:flex; align-items:center; justify-content:center; background:${bgColor}; color:${textColor}; 
                            padding:10px 4px; border-radius:8px; border:1px solid #e0e4e8; text-align:center; min-height:38px; transition: 0.3s;">
                  <span style="font-size:14px; font-weight:600; line-height:1.3;">${name}</span>
                </div>`;
            }
            valCell.innerHTML = html + '</div>';

          // --- ВСЕ ОСТАЛЬНЫЕ РЕГИСТРЫ (ТОЛЬКО ЧТЕНИЕ) ---
          } else {
            valCell.innerHTML = headerHtml + `
              <div style="text-align:center; padding:8px; font-size:18px; font-weight:bold; color:#333; background:#fff; border-radius:8px;">
                ${value}
              </div>`;
          }
        }
      });
    }


    setInterval(updateData, 1000);
    updateData();
    </script>
    )rawliteral";

    html += "</body></html>";
    webServer.send(200, "text/html", html);
}

// 4. Страница автоматизации курятника
void handleAutoChickenHous() {
  String html = "<!DOCTYPE html><html lang='ru'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Управление курятником</title>";
  html += "<style>";
  // Базовые стили для адаптивности и красоты
  html += "body { --content-max:900px; margin: 0; padding: 20px; text-align: center; }";
  html += "h1 { color: #2c3e50; margin-bottom: 30px; font-size: 24px; }";
  // Сетка для кнопок-карточек
  html += ".menu-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; max-width: 900px; margin: 0 auto; }";
  // Стили самих кнопок
  html += ".menu-btn { display: flex; flex-direction: column; align-items: center; justify-content: center; background: white; ";
  html += "border-radius: 12px; padding: 25px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); text-decoration: none; color: #2c3e50; ";
  html += "font-size: 18px; font-weight: bold; transition: transform 0.2s, box-shadow 0.2s; border: 1px solid #e2e8f0; }";
  html += ".menu-btn:hover { transform: translateY(-3px); box-shadow: 0 10px 15px rgba(0,0,0,0.1); border-color: #cbd5e1; }";
  html += ".menu-btn:active { transform: translateY(0); }";
  // Иконки (простые emoji в качестве визуальных якорей)
  html += ".icon { font-size: 32px; margin-bottom: 12px; }";
  // Кастомный цвет для критической кнопки аварий
  html += ".btn-alert { border-left: 5px solid #ef4444; }";
  html += "</style>";
  html += getDesignSystemStyles();
  html += "</head><body>";
  html += getAdminModeBanner();

  html += "<h1>🐔 Автоматизация курятника</h1>";
  
  // Контейнер с кнопками меню
  html += "<div class='menu-grid'>";
  
  html += "  <a href='/climate' class='menu-btn'><span class='icon'>🌡️</span>1. Температура и влажность</a>";
  html += "  <a href='/ventilation' class='menu-btn'><span class='icon'>💨</span>2. Вентиляция</a>";
  html += "  <a href='/door' class='menu-btn'><span class='icon'>🐓</span>3. Лаз</a>";
  html += "  <a href='/feeding' class='menu-btn'><span class='icon'>🌾</span>4. Кормление и поголовье</a>";
  html += "  <a href='/light' class='menu-btn'><span class='icon'>💡</span>5. Освещение и распорядок</a>";
  html += "  <a href='/alerts' class='menu-btn menu-btn-alert'><span class='icon'>⚠️</span>6. Аварии и ошибки</a>";
  html += "  <a href='/' class='menu-btn menu-btn-alert'><span class='icon'>⬅️</span>Назад</a>";
  
  html += "</div>";

  html += "</body></html>";
  
  webServer.send(200, "text/html", html);
}

// 4.1 Освещение и распорядок
void handleLight() {
  String html = "<!DOCTYPE html><html lang='ru'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Освещение и распорядок</title>";
  html += "<style>";
  html += "body { --content-max:600px; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }";
  html += ".container { width: 100%; max-width: 600px; }";
  html += "table { width: 100%; border-collapse: collapse; margin-top: 10px; font-size: 14px; }";
  html += "th, td { padding: 8px; border: 1px solid #eaecf4; text-align: center; }";
  html += "th { background: #f8f9fc; color: #4e73df; font-weight: bold; }";
  html += ".month-name { text-align: left; font-weight: bold; }";
  html += "</style>";
  html += getDesignSystemStyles();
  html += "</head><body>";
  html += getAdminModeBanner();

  html += "<div class='container'>";
  html += "<h2>💡 Освещение и распорядок</h2>";

  // Блок 1: Текущие статусы (Биты)
  html += "<div class='card'>";
  html += "<h3>Текущее состояние</h3>";
  html += "<div class='status-row'><span>Основное освещение (R0:B10):</span><span id='b_0_10' class='badge bg-danger'>Выкл</span></div>";
  html += "<div class='status-row'><span>Дежурное освещение (R0:B11):</span><span id='b_0_11' class='badge bg-danger'>Выкл</span></div>";
  html += "<div class='status-row'><span>Сон по расписанию (R1:B0):</span><span id='b_1_0' class='badge bg-danger'>Выкл</span></div>";
  html += "<div class='status-row'><span>На улице светло (R1:B1):</span><span id='b_1_1' class='badge bg-danger'>Нет</span></div>";
  html += "<div class='status-row'><span>Управление по календарю (R3:B0):</span><span id='b_3_0' class='badge bg-danger'>Выкл</span></div>";
  html += "<div class='status-row'><span>Свет горит постоянно при откл. календаре (R3:B1):</span><span id='b_3_1' class='badge bg-danger'>Нет</span></div>";
  html += "</div>";

  // Блок 2: Временные уставки (Регистры)
  html += "<div class='card'>";
  html += "<h3>Оперативные уставки времени</h3>";
  html += "<div class='status-row'><span>Время восхода солнца (R36):</span><span id='r_36' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Время заката солнца (R37):</span><span id='r_37' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Записанное время подъема (R38):</span><span id='r_38' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Записанное время отбоя (R39):</span><span id='r_39' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Время дежурного освещения (R45):</span><span id='r_45' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Время сумерек (R46):</span><span id='r_46' class='val-box'>--</span></div>";
  html += "</div>";

  // Блок 3: Календарь восходов и закатов (Таблица)
  html += "<div class='card'>";
  html += "<h3>Годовой календарь (Восход / Закат)</h3>";
  html += "<table>";
  html += "<thead><tr><th>Месяц</th><th>Восход (Рег)</th><th>Закат (Рег)</th></tr></thead>";
  html += "<tbody>";
  const char* months[] = {"Январь", "Февраль", "Март", "Апрель", "Май", "Июнь", "Июль", "Август", "Сентябрь", "Октябрь", "Ноябрь", "Декабрь"};
  for (int i = 0; i < 12; i++) {
    int r_voshod = 72 + i;
    int r_zakat = 84 + i;
    html += "<tr>";
    html += "<td class='month-name'>" + String(months[i]) + "</td>";
    html += "<td id='r_" + String(r_voshod) + "'>--</td>";
    html += "<td id='r_" + String(r_zakat) + "'>--</td>";
    html += "</tr>";
  }
  html += "</tbody></table>";
  html += "</div>";

  // Кнопка Назад
  html += "<a href='/autoChickenHous' class='btn'>⬅️ Назад в меню</a>";
  html += "</div>";

  // JavaScript AJAX скрипт для обновления данных в реальном времени
  html += R"rawliteral(
  <script>

  // Функция перевода минут в формат ЧЧ:ММ
  function minToHm(val) {
    if (val === undefined || val === null || val === "") return '--';
    const totalMinutes = parseInt(val);
    const hours = Math.floor(totalMinutes / 60);
    const minutes = totalMinutes % 60;
    return `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}`;
  }

  function updateLightData() {
    fetch('/api/data')
      .then(r => r.json())
      .then(data => {

        // Обновление битовых флагов (0 регистр)
        let r0 = data["0"] || 0;
        updateBitBadge('b_0_10', (r0 >> 10) & 1, "Вкл", "Выкл");
        updateBitBadge('b_0_11', (r0 >> 11) & 1, "Вкл", "Выкл");

        // Обновление битовых флагов (1 регистр)
        let r1 = data["1"] || 0;
        updateBitBadge('b_1_0', (r1 >> 0) & 1, "Вкл", "Выкл");
        updateBitBadge('b_1_1', (r1 >> 1) & 1, "Да", "Нет");

        // Обновление битовых флагов (3 регистр)
        let r3 = data["3"] || 0;
        updateBitBadge('b_3_0', (r3 >> 0) & 1, "Вкл", "Выкл");
        updateBitBadge('b_3_1', (r3 >> 1) & 1, "Да", "Нет");

        // Обновление оперативных регистров времени
        const regList = [36, 37, 38, 39, 45, 46];
        regList.forEach(reg => {
          let el = document.getElementById('r_' + reg);
          if (el && data[reg] !== undefined) el.innerText = minToHm(data[reg]);
        });

        // Обновление календаря (регистры 72-83 и 84-95)
        for (let i = 0; i < 12; i++) {
          let voshodReg = 72 + i;
          let zakatReg = 84 + i;
          
          let voshodEl = document.getElementById('r_' + voshodReg);
          if (voshodEl && data[voshodReg] !== undefined) voshodEl.innerText = minToHm(data[voshodReg]);
          
          let zakatEl = document.getElementById('r_' + zakatReg);
          if (zakatEl && data[zakatReg] !== undefined) zakatEl.innerText = minToHm(data[zakatReg]);
        }
      })
      .catch(err => console.error("Ошибка обновления данных:", err));
  }

  function updateBitBadge(id, state, textOn, textOff) {
    let el = document.getElementById(id);
    if (!el) return;
    if (state === 1) {
      el.innerText = textOn;
      el.className = "badge bg-success";
    } else {
      el.innerText = textOff;
      el.className = "badge bg-danger";
    }
  }

  setInterval(updateLightData, 1000);
  updateLightData();
  </script>
  )rawliteral";

  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

// 4.2 Аварии и ошибки
void handleAlerts() {
  String html = "<!DOCTYPE html><html lang='ru'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Аварии и ошибки</title>";
  html += "<style>";
  html += "body { --content-max:600px; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }";
  html += ".container { width: 100%; max-width: 600px; }";
  html += ".alert-row { display: flex; justify-content: space-between; align-items: center; padding: 12px; border-radius: 8px; margin-bottom: 8px; border: 1px solid #e2e8f0; background: #f8fafc; transition: all 0.3s ease; }";
  html += ".alert-row.active { background: #fef2f2; border-color: #fca5a5; color: #991b1b; font-weight: 600; }";
  html += ".bg-alarm { animation: pulse 2s infinite; }";
  html += ".counter-row { display: flex; justify-content: space-between; align-items: center; padding: 10px 0; border-bottom: 1px solid #f1f5f9; }";
  html += ".counter-row:last-child { border-bottom: none; }";
  html += ".val-box.has-errors { background: #fee2e2; color: #b91c1c; border-color: #fca5a5; }";
  html += "@keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.6; } 100% { opacity: 1; } }";
  html += "</style>";
  html += getDesignSystemStyles();
  html += "</head><body>";
  html += getAdminModeBanner();

  html += "<div class='container'>";
  html += "<h2>⚠️ Аварии и ошибки шлюза</h2>";

  // Блок 1: Активные аварии (Биты)
  html += "<div class='card'>";
  html += "<h3>Критические аварии (Биты)</h3>";
  
  html += "<div id='row_b_1_9' class='alert-row'><span>Авария батарейки (R1:B9)</span><span id='b_1_9' class='badge bg-normal'>Норма</span></div>";
  html += "<div id='row_b_1_10' class='alert-row'><span>Авария датчика температуры (R1:B10)</span><span id='b_1_10' class='badge bg-normal'>Норма</span></div>";
  html += "<div id='row_b_3_5' class='alert-row'><span>Ошибка расчета вентиляции (R3:B5)</span><span id='b_3_5' class='badge bg-normal'>Норма</span></div>";
  html += "<div id='row_b_3_9' class='alert-row'><span>Ошибка при кормлении (R3:B9)</span><span id='b_3_9' class='badge bg-normal'>Норма</span></div>";
  
  html += "</div>";

  // Блок 2: Счетчики ошибок (Регистры)
  html += "<div class='card'>";
  html += "<h3>Статистика неисправностей (Регистры)</h3>";
  
  html += "<div class='counter-row'><span>Ошибок по кормушке К1 (R48):</span><span id='r_48' class='val-box'>0</span></div>";
  html += "<div class='counter-row'><span>Ошибок по кормушке К2 (R49):</span><span id='r_49' class='val-box'>0</span></div>";
  html += "<div class='counter-row'><span>Ошибок по кормушке К3 (R50):</span><span id='r_50' class='val-box'>0</span></div>";
  html += "<div class='counter-row'><span>Ошибок по кормушке К4 (R51):</span><span id='r_51' class='val-box'>0</span></div>";
  html += "<div class='counter-row'><span>Ошибки от модуля Wi-Fi (R52):</span><span id='r_52' class='val-box'>0</span></div>";
  
  html += "</div>";

  // Кнопка Назад
  html += "<a href='/autoChickenHous' class='btn'>⬅️ Назад в меню</a>";
  html += "</div>";

  // JavaScript скрипт динамического обновления
  html += R"rawliteral(
  <script>
  function updateAlertsData() {
    fetch('/api/data')
      .then(r => r.json())
      .then(data => {
        // Чтение регистров для битовых масок
        let r1 = data["1"] || 0;
        let r3 = data["3"] || 0;

        // Проверка битов 1 регистра
        updateAlarmState('b_1_9', 'row_b_1_9', (r1 >> 9) & 1, "АВАРИЯ");
        updateAlarmState('b_1_10', 'row_b_1_10', (r1 >> 10) & 1, "АВАРИЯ");

        // Проверка битов 3 регистра
        updateAlarmState('b_3_5', 'row_b_3_5', (r3 >> 5) & 1, "ОШИБКА");
        updateAlarmState('b_3_9', 'row_b_3_9', (r3 >> 9) & 1, "ОШИБКА");

        // Обновление числовых регистров (48-52)
        const counterRegs = [48, 49, 50, 51, 52];
        counterRegs.forEach(reg => {
          let el = document.getElementById('r_' + reg);
          if (el && data[reg] !== undefined) {
            let val = data[reg];
            el.innerText = val;
            
            // Если количество ошибок больше 0, подсвечиваем поле
            if (val > 0) {
              el.className = "val-box has-errors";
            } else {
              el.className = "val-box";
            }
          }
        });
      })
      .catch(err => console.error("Ошибка при получении данных аварий:", err));
  }

  function updateAlarmState(badgeId, rowId, isActive, alarmText) {
    let badge = document.getElementById(badgeId);
    let row = document.getElementById(rowId);
    if (!badge || !row) return;

    if (isActive === 1) {
      badge.innerText = alarmText;
      badge.className = "badge bg-alarm";
      row.className = "alert-row active";
    } else {
      badge.innerText = "Норма";
      badge.className = "badge bg-normal";
      row.className = "alert-row";
    }
  }

  setInterval(updateAlertsData, 1000);
  updateAlertsData();
  </script>
  )rawliteral";

  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

// 4.4 Кормление и поголовье
void handleFeeding() {
  String html = "<!DOCTYPE html><html lang='ru'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Кормление и поголовье</title>";
  html += "<style>";
  html += "body { --content-max:700px; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }";
  html += ".container { width: 100%; max-width: 700px; }";
  html += ".grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }";
  html += "table { width: 100%; border-collapse: collapse; margin-top: 10px; font-size: 14px; }";
  html += "th, td { padding: 8px; border: 1px solid #eaecf4; text-align: center; }";
  html += "th { background: #f8f9fc; color: #27ae60; font-weight: bold; }";
  html += "@media(max-width: 600px) { .grid-2 { grid-template-columns: 1fr; } }";
  html += "</style>";
  html += getDesignSystemStyles();
  html += "</head><body>";
  html += getAdminModeBanner();

  html += "<div class='container'>";
  html += "<h2>🌾 Кормление и поголовье птицы</h2>";

  // Блок 1: Поголовье и общая статистика
  html += "<div class='card'>";
  html += "<h3>Статистика и поголовье</h3>";
  html += "<div class='grid-2'>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Количество птиц (R57):</span><span id='r_57' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Масса одной птицы (R56):</span><span id='r_56' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Пройдено кормлений (R14):</span><span id='r_14' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>План кормлений (R15):</span><span id='r_15' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "</div>";
  html += "</div>";

  // Блок 2: Состояние процесса кормления (Биты регистра 3)
  html += "<div class='card'>";
  html += "<h3>Статус процесса кормления</h3>";
  html += "<div class='status-row'><span>Пропуски кормления (R3:B8):</span><span id='b_3_8' class='badge bg-danger'>Есть</span></div>";
  html += "<div class='status-row'><span>Кормление за сутки закончилось (R3:B11):</span><span id='b_3_11' class='badge bg-neutral'>Нет</span></div>";
  html += "<div class='status-row'><span>Количество кормлений равно нулю (R3:B12):</span><span id='b_3_12' class='badge bg-danger'>Да</span></div>";
  html += "<div class='status-row'><span>Включен режим ручного кормления (R3:B14):</span><span id='b_3_14' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Отработка пропусков кормления (R3:B15):</span><span id='b_3_15' class='badge bg-neutral'>Выкл</span></div>";
  html += "</div>";

  // Блок 3: Следующее кормление
  html += "<div class='card'>";
  html += "<h3>Следующее кормление</h3>";
  html += "<div class='status-row'><span>Номер кормушки (R12):</span><span id='r_12' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Время начала (R16):</span><span id='r_16' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Длительность, мин (R13):</span><span id='r_13' class='val-box'>--</span></div>";
  html += "</div>";

  // Блок 4: Статус оборудования (Кормушки и модули)
  html += "<div class='card'>";
  html += "<h3>Статус оборудования (Кормушки 1-4)</h3>";
  html += "<table>";
  html += "<thead><tr><th>Устройство</th><th>Подключение к системе</th><th>Питание / Сеть</th></tr></thead>";
  html += "<tbody>";
  for (int i = 1; i <= 4; i++) {
    html += "<tr>";
    html += "<td>Кормушка " + String(i) + "</td>";
    html += "<td><span id='b_4_" + String(i-1) + "' class='badge bg-danger'>Откл</span></td>";
    html += "<td><span id='b_4_" + String(8 + (i-1)) + "' class='badge bg-danger'>Выкл</span></td>";
    html += "</tr>";
  }
  html += "<tr>";
  html += "<td>Новый модуль</td>";
  html += "<td colspan='2'><span id='b_4_13' class='badge bg-danger'>Выкл</span></td>";
  html += "</tr>";
  html += "</tbody></table>";
  html += "</div>";

  // Блок 5: Расписание кормлений (Таблица 1-15)
  html += "<div class='card'>";
  html += "<h3>График и длительность кормлений (1-15)</h3>";
  html += "<table>";
  html += "<thead><tr><th>№</th><th>Время кормления</th><th>Длительность</th></tr></thead>";
  html += "<tbody>";
  for (int i = 1; i <= 15; i++) {
    int r_time = 110 + (i - 1);
    int r_dur = 125 + (i - 1);
    html += "<tr>";
    html += "<td>" + String(i) + "</td>";
    html += "<td id='r_" + String(r_time) + "'>--</td>";
    html += "<td id='r_" + String(r_dur) + "'>--</td>";
    html += "</tr>";
  }
  html += "</tbody></table>";
  html += "</div>";

  // Кнопка Назад
  html += "<a href='/autoChickenHous' class='btn'>⬅️ Назад в меню</a>";
  html += "</div>";

  // JavaScript AJAX скрипт динамического обновления
  html += R"rawliteral(
  <script>
  function minToHm(val) {
    if (val === undefined || val === null || val === "") return '--';
    const totalMinutes = parseInt(val);
    const hours = Math.floor(totalMinutes / 60);
    const minutes = totalMinutes % 60;
    return `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}`;
  }

  function updateFeedingData() {
    fetch('/api/data')
      .then(r => r.json())
      .then(data => {
        // Загрузка сырых регистров
        let r3 = data["3"] || 0;
        let r4 = data["4"] || 0;

        // Обновление битовых статусов регистра 3
        updateBadge('b_3_8', (r3 >> 8) & 1, "ПРОПУСК", "Норма", true);
        updateBadge('b_3_11', (r3 >> 11) & 1, "Да", "Нет", false);
        updateBadge('b_3_12', (r3 >> 12) & 1, "ДА (0)", "В норме", true);
        updateBadge('b_3_14', (r3 >> 14) & 1, "РУЧНОЙ", "Авто", false);
        updateBadge('b_3_15', (r3 >> 15) & 1, "ОТРАБОТКА", "Нет", false);

        // Обновление битовых статусов регистра 4 (Кормушки 1-4)
        for (let i = 0; i < 4; i++) {
          updateBadge('b_4_' + i, (r4 >> i) & 1, "Подкл", "Откл", false);
          updateBadge('b_4_' + (8 + i), (r4 >> (8 + i)) & 1, "В СЕТИ", "ВЫКЛ", false);
        }
        updateBadge('b_4_13', (r4 >> 13) & 1, "В СЕТИ", "ВЫКЛ", false);

        // Оперативные регистры общего состояния
        if (data[12] !== undefined) document.getElementById('r_12').innerText = data[12]; // Номер кормушки
        if (data[14] !== undefined) document.getElementById('r_14').innerText = data[14]; // Пройдено за сутки
        if (data[15] !== undefined) document.getElementById('r_15').innerText = data[15]; // План
        if (data[56] !== undefined) document.getElementById('r_56').innerText = data[56]; // Масса
        if (data[57] !== undefined) document.getElementById('r_57').innerText = data[57]; // Кол-во птиц

        // Форматирование времени для текущих уставок кормления
        if (data[13] !== undefined) document.getElementById('r_13').innerText = (parseInt(data[13]) / 10).toFixed(1) + " сек"; // Длительность (Рег 13)
        if (data[16] !== undefined) document.getElementById('r_16').innerText = minToHm(data[16]); // Время следующего (Рег 16)

        // Заполнение таблицы расписания (110-124 и 125-139)
        for (let i = 0; i < 15; i++) {
          let tReg = 110 + i;
          let dReg = 125 + i;

          let tEl = document.getElementById('r_' + tReg);
          if (tEl && data[tReg] !== undefined) tEl.innerText = minToHm(data[tReg]);

          let dEl = document.getElementById('r_' + dReg);
          if (dEl && data[dReg] !== undefined) {
            dEl.innerText = (parseInt(data[dReg]) / 10).toFixed(1) + " сек";
          }
        }
      })
      .catch(err => console.error("Ошибка обновления данных кормления:", err));
  }

  function updateBadge(id, state, textOn, textOff, isAlarmType) {
    let el = document.getElementById(id);
    if (!el) return;
    if (state === 1) {
      el.innerText = textOn;
      el.className = isAlarmType ? "badge bg-danger" : "badge bg-success";
    } else {
      el.innerText = textOff;
      el.className = isAlarmType ? "badge bg-success" : "badge bg-danger";
    }
  }

  setInterval(updateFeedingData, 1000);
  updateFeedingData();
  </script>
  )rawliteral";

  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

// 4.5 Лаз (дверь)
void handleDoor() {
  String html = "<!DOCTYPE html><html lang='ru'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Управление лазом (дверью)</title>";
  html += "<style>";
  html += "body { --content-max:650px; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }";
  html += ".container { width: 100%; max-width: 650px; }";
  html += ".lock-status { display: inline-block; max-width: 65%; white-space: normal; line-height: 1.25; }";
  html += ".grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }";
  html += "@media(max-width: 600px) { .grid-2 { grid-template-columns: 1fr; } }";
  html += "</style>";
  html += getDesignSystemStyles();
  html += "</head><body>";
  html += getAdminModeBanner();

  html += "<div class='container'>";
  html += "<h2>🐓 Автоматика лаза (двери)</h2>";

  // Блок 1: Физическое состояние и датчики
  html += "<div class='card'>";
  html += "<h3>Текущее состояние и датчики</h3>";
  html += "<div class='status-row'><span>Положение лаза (R2:B8–B9):</span><div><span id='b_2_8' class='badge bg-neutral'>Открыт</span> <span id='b_2_9' class='badge bg-neutral'>Крайнее</span></div></div>";
  html += "<div class='status-row'><span>Физический датчик открытой двери (R1:B2):</span><span id='b_1_2' class='badge bg-neutral'>Датчик</span></div>";
  html += "<div class='status-row'><span>Опрос концевого датчика двери (R1:B12):</span><span id='b_1_12' class='badge bg-neutral'>Нет</span></div>";
  html += "<div class='status-row'><span>Состояние движения (R2:B11):</span><span id='b_2_11' class='badge bg-neutral'>Стоит</span></div>";
  html += "<div class='status-row'><span>Направление движения (R0):</span><div><span id='b_0_9' class='badge bg-neutral'>Вверх ⬆️</span> <span id='b_0_8' class='badge bg-neutral'>Вниз ⬇️</span></div></div>";
  html += "<div class='status-row'><span>Лаз зафиксирован (R2:B12):</span><span id='b_2_12' class='badge bg-neutral lock-status'>Нет</span></div>";
  html += "<div class='status-row'><span>Занято направлением движения лаза (R2:B13):</span><span id='b_2_13' class='badge bg-neutral'>Нет</span></div>";
  html += "<div class='status-row'><span>Занято направлением движения (R2:B14):</span><span id='b_2_14' class='badge bg-neutral'>Нет</span></div>";
  html += "</div>";

  // Блок 2: Режимы работы и температурные блокировки
  html += "<div class='card'>";
  html += "<h3>Режимы и блокировки управления</h3>";
  html += "<div class='status-row'><span>Режим управления лазом (R2:B10):</span><span id='b_2_10' class='badge bg-info'>Авто</span></div>";
  html += "<div class='status-row'><span>Управление лазом по календарю (R2:B0):</span><span id='b_2_0' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Управление закрытием по расписанию (R2:B1):</span><span id='b_2_1' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Управление лазом по температуре (R2:B15):</span><span id='b_2_15' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Запрет открытия по темп. внутри (R2:B6):</span><span id='b_2_6' class='badge bg-neutral'>Нет</span></div>";
  html += "<div class='status-row'><span>Запрет открытия по темп. на улице (R2:B7):</span><span id='b_2_7' class='badge bg-neutral'>Нет</span></div>";
  html += "</div>";

  // Блок 3: Конфигурация времени и температур
  html += "<div class='card'>";
  html += "<h3>Временные и температурные уставки</h3>";
  html += "<div class='grid-2'>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Расчетное откр. (R10):</span><span id='r_10' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Расчетное закр. (R9):</span><span id='r_9' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Записанное откр. (R40):</span><span id='r_40' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Записанное закр. (R41):</span><span id='r_41' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Время отлож. закр. (R42):</span><span id='r_42' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Время хода привода (R43):</span><span id='r_43' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Темп. открытия (R33):</span><span id='r_33' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "</div>";
  html += "</div>";

  // Кнопка Назад
  html += "<a href='/autoChickenHous' class='btn'>⬅️ Назад в меню</a>";
  html += "</div>";

  // JavaScript AJAX скрипт динамического обновления
  html += R"rawliteral(
  <script>
  function minToHm(val) {
    if (val === undefined || val === null || val === "") return '--';
    const totalMinutes = parseInt(val);
    const hours = Math.floor(totalMinutes / 60);
    const minutes = totalMinutes % 60;
    return `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}`;
  }

  function updateDoorData() {
    fetch('/api/data')
      .then(r => r.json())
      .then(data => {
        let r0 = data["0"] || 0;
        let r1 = data["1"] || 0;
        let r2 = data["2"] || 0;

        // Разбор флагов движения (Регистр 0)
        updateBadge('b_0_8', (r0 >> 8) & 1, "АКТИВНО", "Выкл", "bg-danger", "bg-neutral");
        updateBadge('b_0_9', (r0 >> 9) & 1, "АКТИВНО", "Выкл", "bg-success", "bg-neutral");

        // Концевик и опрос датчика (Регистр 1)
        updateBadge('b_1_2', (r1 >> 2) & 1, "ОТКРЫТА", "Закрыта", "bg-success", "bg-danger");
        updateBadge('b_1_12', (r1 >> 12) & 1, "ОПРОС", "Нет", "bg-info", "bg-neutral");

        // Статусы и режимы лаза (Регистр 2)
        updateBadge('b_2_0', (r2 >> 0) & 1, "Вкл", "Выкл", "bg-success", "bg-neutral");
        updateBadge('b_2_1', (r2 >> 1) & 1, "Вкл", "Выкл", "bg-success", "bg-neutral");
        updateBadge('b_2_6', (r2 >> 6) & 1, "ЗАПРЕТ", "Нет", "bg-danger", "bg-success");
        updateBadge('b_2_7', (r2 >> 7) & 1, "ЗАПРЕТ", "Нет", "bg-danger", "bg-success");
        updateBadge('b_2_8', (r2 >> 8) & 1, "ОТКРЫТ", "ЗАКРЫТ", "bg-success", "bg-danger");
        updateBadge('b_2_9', (r2 >> 9) & 1, "КРАЙНЕЕ", "ПРОМЕЖУТОЧНОЕ", "bg-info", "bg-neutral");
        updateBadge('b_2_10', (r2 >> 10) & 1, "РУЧНОЙ", "Авто", "bg-danger", "bg-info");
        updateBadge('b_2_11', (r2 >> 11) & 1, "ДВИЖЕНИЕ", "Стоит", "bg-danger", "bg-neutral");
        const isHatchLocked = (r2 >> 12) & 1;
        const isAtExtremePosition = (r2 >> 9) & 1;
        const isOpen = (r2 >> 8) & 1;
        const lockStatus = document.getElementById('b_2_12');
        if (lockStatus) {
          if (!isHatchLocked) {
            lockStatus.innerText = "Нет";
            lockStatus.className = "badge bg-neutral lock-status";
          } else if (!isAtExtremePosition) {
            lockStatus.innerText = "ЛАЗ ЗАФИКСИРОВАН В ПРОМЕЖУТОЧНОМ ПОЛОЖЕНИИ!";
            lockStatus.className = "badge bg-danger lock-status";
          } else if (isOpen) {
            lockStatus.innerText = "ЛАЗ ЗАБЛОКИРОВАН ОТКРЫТЫМ!";
            lockStatus.className = "badge bg-danger lock-status";
          } else {
            lockStatus.innerText = "ЛАЗ ЗАБЛОКИРОВАН ЗАКРЫТЫМ!";
            lockStatus.className = "badge bg-danger lock-status";
          }
        }
        updateBadge('b_2_13', (r2 >> 13) & 1, "ЗАНЯТО", "Нет", "bg-danger", "bg-neutral");
        updateBadge('b_2_14', (r2 >> 14) & 1, "ЗАНЯТО", "Нет", "bg-danger", "bg-neutral");
        updateBadge('b_2_15', (r2 >> 15) & 1, "Вкл", "Выкл", "bg-success", "bg-neutral");

        const timeRegs = [9, 10, 40, 41, 42]; // Регистры абсолютного времени суток
        timeRegs.forEach(reg => {
          let el = document.getElementById('r_' + reg);
          if (el && data[reg] !== undefined) el.innerText = minToHm(data[reg]);
        });

        // Регистр 33 — Температура открытия лаза
        let el33 = document.getElementById('r_33');
        if (el33 && data[33] !== undefined) el33.innerText = data[33] + "°C";

        // Регистр 43 — Время хода привода в секундах
        let el43 = document.getElementById('r_43');
        if (el43 && data[43] !== undefined) el43.innerText = data[43] + " сек";
      })
      .catch(err => console.error("Ошибка обновления данных лаза:", err));
  }

  function updateBadge(id, state, textOn, textOff, classOn, classOff) {
    let el = document.getElementById(id);
    if (!el) return;
    if (state === 1) {
      el.innerText = textOn;
      el.className = "badge " + classOn;
    } else {
      el.innerText = textOff;
      el.className = "badge " + classOff;
    }
  }

  setInterval(updateDoorData, 1000);
  updateDoorData();
  </script>
  )rawliteral";

  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

// 4.6 Вентиляция
void handleVentilation() {
  String html = "<!DOCTYPE html><html lang='ru'><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Вентиляция</title><style>body { --content-max:700px; margin:0; padding:20px; display:flex; flex-direction:column; align-items:center; } .container { width:100%; max-width:700px; } .grid-2 { display:grid; grid-template-columns:1fr 1fr; gap:12px; } @media(max-width:650px) { .grid-2 { grid-template-columns:1fr; } }</style>";
  html += getDesignSystemStyles();
  html += "</head><body>";
  html += getAdminModeBanner();
  html += "<div class='container'><h2>💨 Вентиляция</h2>";

  html += "<div class='card'><h3>Текущее состояние</h3><div class='grid-2'><div>";
  html += "<div class='status-row'><span>Приточный вентилятор (R0:B12)</span><span id='b_0_12' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Вытяжной вентилятор (R0:B13)</span><span id='b_0_13' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Количество вентиляторов (R53)</span><span id='r_53' class='val-box'>--</span></div>";
  html += "</div><div>";
  html += "<div class='status-row'><span>Производительность вентиляторов (R54)</span><span id='r_54' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Объем помещения (R55)</span><span id='r_55' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Охлаждение (R3:B7)</span><span id='b_3_7' class='badge bg-neutral'>Выкл</span></div>";
  html += "</div></div></div>";

  html += "<div class='card'><h3>Режимы управления</h3>";
  html += "<div class='status-row'><span>Ручное управление вентиляцией (R1:B15)</span><span id='b_1_15' class='badge bg-neutral'>Авто</span></div>";
  html += "<div class='status-row'><span>Автоматическое проветривание (R1:B3)</span><span id='b_1_3' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Ручное проветривание (R1:B4)</span><span id='b_1_4' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Работа вентиляторов днем (R1:B13)</span><span id='b_1_13' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Работа вентиляторов во время сна (R1:B14)</span><span id='b_1_14' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Коррекция вентиляторов (R1:B6)</span><span id='b_1_6' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Управление по ГОСТ (R2:B4)</span><span id='b_2_4' class='badge bg-neutral'>Выкл</span></div></div>";

  html += "<div class='card'><h3>Загазованность и параметры проветривания</h3>";
  html += "<div class='status-row'><span>Датчик загазованности (R0:B2)</span><span id='b_0_2' class='badge bg-success'>Норма</span></div>";
  html += "<div class='status-row'><span>Опрос датчика газа (R1:B11)</span><span id='b_1_11' class='badge bg-neutral'>Нет</span></div>";
  html += "<div class='status-row'><span>Превышение лимита газа в час (R1:B8)</span><span id='b_1_8' class='badge bg-success'>Норма</span></div>";
  html += "<div class='grid-2'><div>";
  html += "<div class='status-row'><span>Срабатывания газа за час (R31)</span><span id='r_31' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Всего срабатываний газа (R32)</span><span id='r_32' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Включений вентиляторов при газе (R44)</span><span id='r_44' class='val-box'>--</span></div>";
  html += "</div><div>";
  html += "<div class='status-row'><span>Проветриваний за час (R34)</span><span id='r_34' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Допустимо проветриваний в час (R58)</span><span id='r_58' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Время смены воздуха, сек (R35)</span><span id='r_35' class='val-box'>--</span></div>";
  html += "</div></div></div>";

  html += "<div class='card'><h3>Расчетный объем свежего воздуха (м³)</h3><table><thead><tr><th>Режим расчета</th><th>Лето</th><th>Зима</th><th>Осень</th></tr></thead><tbody>";
  html += "<tr><td><b>По ГОСТ</b></td><td id='r_60'>--</td><td id='r_61'>--</td><td id='r_62'>--</td></tr>";
  html += "<tr><td><b>Текущий расчет</b></td><td id='r_63'>--</td><td id='r_64'>--</td><td id='r_65'>--</td></tr></tbody></table></div>";
  html += "<a href='/autoChickenHous' class='btn'>⬅️ Назад в меню</a></div>";

  html += R"rawliteral(<script>
    function setBadge(id, active, onText, offText, onClass = 'bg-success', offClass = 'bg-neutral') {
      const el = document.getElementById(id);
      if (!el) return;
      el.textContent = active ? onText : offText;
      el.className = 'badge ' + (active ? onClass : offClass);
    }
    function updateVentilation() {
      fetch('/api/data').then(response => response.json()).then(data => {
        const r0 = Number(data['0']) || 0;
        const r1 = Number(data['1']) || 0;
        const r2 = Number(data['2']) || 0;
        const r3 = Number(data['3']) || 0;
        setBadge('b_0_12', (r0 >> 12) & 1, 'Работает', 'Выкл');
        setBadge('b_0_13', (r0 >> 13) & 1, 'Работает', 'Выкл');
        setBadge('b_0_2', (r0 >> 2) & 1, 'Газ обнаружен', 'Норма', 'bg-danger', 'bg-success');
        setBadge('b_1_11', (r1 >> 11) & 1, 'Опрос', 'Нет', 'bg-info', 'bg-neutral');
        setBadge('b_1_15', (r1 >> 15) & 1, 'Ручной', 'Авто', 'bg-danger', 'bg-success');
        setBadge('b_1_3', (r1 >> 3) & 1, 'Вкл', 'Выкл');
        setBadge('b_1_4', (r1 >> 4) & 1, 'Вкл', 'Выкл');
        setBadge('b_1_6', (r1 >> 6) & 1, 'Активна', 'Выкл', 'bg-info', 'bg-neutral');
        setBadge('b_1_8', (r1 >> 8) & 1, 'Превышение', 'Норма', 'bg-danger', 'bg-success');
        setBadge('b_1_13', (r1 >> 13) & 1, 'Вкл', 'Выкл', 'bg-info', 'bg-neutral');
        setBadge('b_1_14', (r1 >> 14) & 1, 'Вкл', 'Выкл', 'bg-info', 'bg-neutral');
        setBadge('b_2_4', (r2 >> 4) & 1, 'По ГОСТ', 'Выкл');
        setBadge('b_3_7', (r3 >> 7) & 1, 'Вкл', 'Выкл');
        [31,32,34,35,44,53,54,55,58,60,61,62,63,64,65].forEach(reg => {
          const el = document.getElementById('r_' + reg);
          if (el && data[reg] !== undefined) el.textContent = data[reg];
        });
      }).catch(error => console.error('Ошибка обновления вентиляции:', error));
    }
    updateVentilation();
    setInterval(updateVentilation, 1000);
  </script>)rawliteral";

  html += "</body></html>";
  webServer.send(200, "text/html; charset=utf-8", html);
}

// 4.7 Температура и влажность
void handleClimate() {
  String html = "<!DOCTYPE html><html lang='ru'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Температура и влажность</title>";
  html += "<style>";
  html += "body { --content-max:800px; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }";
  html += ".container { width: 100%; max-width: 800px; }";
  html += ".grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }";
  html += ".grid-3 { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 15px; }";
  html += "table { width: 100%; border-collapse: collapse; margin-top: 10px; font-size: 14px; }";
  html += "th, td { padding: 8px; border: 1px solid #e2e8f0; text-align: center; }";
  html += "th { background: #f8fafc; color: #0284c7; font-weight: bold; }";
  html += "@media(max-width: 768px) { .grid-2, .grid-3 { grid-template-columns: 1fr; } }";
  html += "</style>";
  html += getDesignSystemStyles();
  html += "</head><body>";
  html += getAdminModeBanner();

  html += "<div class='container'>";
  html += "<h2>🌡️ Температура и влажность</h2>";

  // Блок 1: Текущие метеоданные и датчики
  html += "<div class='card'>";
  html += "<h3>Текущие измерения и датчики</h3>";
  html += "<div class='grid-2'>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Темп. в курятнике (R5):</span><span id='r_5' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Влажность в курятнике (R6):</span><span id='r_6' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Кол-во датчиков темп. (R28):</span><span id='r_28' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Темп. на улице (R7):</span><span id='r_7' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Влажность на улице (R8):</span><span id='r_8' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Темп. < 0°C на улице (R0:B3):</span><span id='b_0_3' class='badge bg-neutral'>Нет</span></div>";
  html += "    <div class='status-row'><span>Темп. < 0°C в доме (R0:B4):</span><span id='b_0_4' class='badge bg-neutral'>Нет</span></div>";
  html += "  </div>";
  html += "</div>";
  html += "</div>";

  // Блок 2: Состояние исполнительных устройств
  html += "<div class='card'>";
  html += "<h3>Исполнительные устройства</h3>";
  html += "<div class='grid-2'>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Нагрев (R0:B14):</span><span id='b_0_14' class='badge bg-neutral'>Выкл</span></div>";
  html += "    <div class='status-row'><span>Охлаждение (R0:B0):</span><span id='b_0_0' class='badge bg-neutral'>Выкл</span></div>";
  html += "    <div class='status-row'><span>Управление охлаждением (R3:B7):</span><span id='b_3_7' class='badge bg-neutral'>Выкл</span></div>";
  html += "  </div>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Управление осушителем (R1:B7):</span><span id='b_1_7' class='badge bg-neutral'>Выкл</span></div>";
  html += "    <div class='status-row'><span>Используется осушитель (R4:B4):</span><span id='b_4_4' class='badge bg-neutral'>Нет</span></div>";
  html += "    <div class='status-row'><span>Осушитель в сети (R4:B12):</span><span id='b_4_12' class='badge bg-neutral'>Выкл</span></div>";
  html += "    <div class='status-row'><span>Раб. нагревателя за час (R47):</span><span id='r_47' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "</div>";
  html += "</div>";

  // Блок 3: Уставки температуры и влажности
  html += "<div class='card'>";
  html += "<h3>Временные и целевые уставки</h3>";
  html += "<div class='grid-2'>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Заданная температура (R24):</span><span id='r_24' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Отклонение темп. (R25):</span><span id='r_25' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Заданная темп. днем (R29):</span><span id='r_29' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Температура охлаждения (R30):</span><span id='r_30' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Заданная влажность (R26):</span><span id='r_26' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Отклонение влажности (R27):</span><span id='r_27' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "</div>";
  html += "</div>";

  // Кнопка Назад
  html += "<a href='/autoChickenHous' class='btn'>⬅️ Назад в меню</a>";
  html += "</div>";

  // Внедрение JavaScript для автоматического фонового AJAX-опроса по API шлюза
  html += R"rawliteral(<script>
    function updateClimateData() {
        fetch('/api/data')
        .then(r => r.json())
        .then(data => {
            let r0 = data['0'] || 0;
            let r1 = data['1'] || 0;
            let r3 = data['3'] || 0;
            let r4 = data['4'] || 0;

            const signIn = (r0 >> 4) & 1;  // Бит 4 — минус внутри
            const signOut = (r0 >> 3) & 1; // Бит 3 — минус на улице

            updateBadge('b_0_0', (r0 >> 0) & 1, 'АКТИВНО', 'Выкл', 'bg-danger', 'bg-neutral');
            updateBadge('b_0_3', (r0 >> 3) & 1, '< 0°C', 'Нет', 'bg-info', 'bg-neutral');
            updateBadge('b_0_4', (r0 >> 4) & 1, '< 0°C', 'Нет', 'bg-info', 'bg-neutral');
            updateBadge('b_0_14', (r0 >> 14) & 1, 'НАГРЕВ', 'Выкл', 'bg-danger', 'bg-neutral');
            updateBadge('b_1_7', (r1 >> 7) & 1, 'Вкл', 'Выкл', 'bg-success', 'bg-neutral');
            updateBadge('b_3_7', (r3 >> 7) & 1, 'Вкл', 'Выкл', 'bg-success', 'bg-neutral');
            updateBadge('b_4_4', (r4 >> 4) & 1, 'Да', 'Нет', 'bg-success', 'bg-neutral');
            updateBadge('b_4_12', (r4 >> 12) & 1, 'В СЕТИ', 'Выкл', 'bg-success', 'bg-neutral');
            const climateRegs = [5, 6, 7, 8, 24, 25, 26, 27, 28, 29, 30, 47];
            climateRegs.forEach(reg => {
                let el = document.getElementById('r_' + reg);
                if (el && data[reg] !== undefined) {
                    let val = data[reg];

                    // Если это датчик курятника (5) или улицы (7) — инвертируем знак при активном бите
                    if (reg === 5 && signIn === 1) val = -val;
                    if (reg === 7 && signOut === 1) val = -val;

                    if (reg===5||reg===7||reg===24||reg===25||reg===29||reg===30) { el.innerText = val + ' °C'; }
                    else if (reg===6||reg===8||reg===26||reg===27) { el.innerText = val + ' %'; }
                    else { el.innerText = val; }
                }
            });
        })
        .catch(err => console.error('Ошибка обновления:', err));
    }
    function updateBadge(id, state, textOn, textOff, classOn, classOff) {
        let el = document.getElementById(id);
        if (!el) return;
        if (state === 1) { el.innerText = textOn; el.className = 'badge ' + classOn; }
        else { el.innerText = textOff; el.className = 'badge ' + classOff; }
    }
    setInterval(updateClimateData, 1000);
    updateClimateData();
    </script>)rawliteral";

  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

// 5. Страница настроек
void handleSettings() {
  String html = getHeader("Настройки шлюза");
  
  // Дополнительно форсируем ограничение ширины для всех внутренних элементов карточек
  //html += "<style>.card { max-width: 100%; box-sizing: border-box; }</style>";

  // Стили для карточки связи, индикаторов сигнала и MQTT
  html += R"rawliteral(
  <style>
    .card { max-width: 100%; box-sizing: border-box; }
    .status-card { margin-bottom: 20px; }
    .ip-info { font-size: 0.85em; color: #666; margin-top: 5px; line-height: 1.5; text-align: left; }
    .ip-info b { color: #333; font-family: monospace; }
    .sig-box { display: flex; align-items: flex-end; height: 17px; gap: 3px; margin-top: 5px; }
    .bar { width: 4px; background: #e0e0e0; border-radius: 1px; transition: 0.3s; }
    .b1 { height: 5px; } .b2 { height: 9px; } .b3 { height: 13px; } .b4 { height: 17px; }
    .green { background: #28a745 !important; }
    .yellow { background: #ffc107 !important; }
    .red { background: #dc3545 !important; }
    .dot { height: 14px; width: 14px; border-radius: 50%; display: inline-block; margin-right: 8px; background: #bbb; vertical-align: middle; }
    .online { background: #28a745; box-shadow: 0 0 8px rgba(40,167,69,0.4); }
    .offline { background: #dc3545; }
  </style>
  )rawliteral";

  // --- ПЕРЕНЕСЕННАЯ КАРТОЧКА: ПОДРОБНАЯ ИНФОРМАЦИЯ О СВЯЗИ ---
  html += "<div class='card status-card'>";
  html += "<h3>📊 Текущий статус связи</h3>";

  // WiFi
  html += "<div class='status-row'><div><span>Сеть: <b id='ssid-name'>" + (sta_ssid != "" ? escapeHtml(sta_ssid) : "Ожидание...") + "</b></span>";
  html += "<div class='ip-info'>Локальный IP: <b id='sta-ip'>...</b><br>Точка (AP) IP: <b>" + WiFi.softAPIP().toString() + "</b></div></div>";
  html += "<div class='sig-box' id='wifi-bars'><div class='bar b1'></div><div class='bar b2'></div><div class='bar b3'></div><div class='bar b4'></div></div></div>";
  
  // MQTT
  html += "<div class='status-row'><span>Статус MQTT:</span><span><span id='mqtt-dot' class='dot'></span><b id='mqtt-stat'>Подключение...</b></span></div>";
  html += "</div>";

  // Блок Wi-Fi
  html += "<div class='card'><h3>📶 Сеть Wi-Fi</h3>";
  html += "<button type='button' onclick='scan(this)' class='btn' style='width:100%'>🔄 Сканировать сети</button>";
  html += "<label class='field-label' for='netList'>Доступные сети Wi-Fi</label>";
  html += "<select id='netList' onchange='document.getElementsByName(\"ssid\")[0].value=this.value'></select>";
  html += "<form action='/saveWiFi' method='POST'>";
  html += "<label class='field-label' for='wifi-ssid'>Имя сети (SSID)</label><input id='wifi-ssid' type='text' name='ssid' placeholder='SSID роутера' required>";
  html += "<label class='field-label' for='wifi-password'>Пароль сети</label><input id='wifi-password' type='password' name='pass' placeholder='Пароль' required>";
  html += "<input type='submit' value='💾 Сохранить и перезагрузить' class='btn' style='background:#28a745; width:100%'>";
  html += "</form>";
  html += "</div>";

  // Блок MQTT
  html += "<div class='card'><h3>☁️ Параметры MQTT</h3>";
  html += "<form action='/saveMQTT' method='POST'>";
  html += "<label class='field-label' for='mqtt-user'>Пользователь MQTT</label><input id='mqtt-user' type='text' name='mq_user' value='" + escapeHtml(mqtt_user) + "' placeholder='Пользователь' autocomplete='username'>";
  html += "<label class='field-label' for='mqtt-password'>Пароль MQTT</label><p class='field-hint'>" + String(mqtt_pass.length() ? "Пароль сохранён" : "Пароль не задан") + ". Оставьте поле пустым, чтобы не менять его.</p>";
  html += "<input id='mqtt-password' type='password' name='mq_pass' value='' placeholder='Новый пароль (необязательно)' autocomplete='new-password'>";
  html += "<label class='field-label' for='mqtt-host'>Адрес брокера MQTT</label><input id='mqtt-host' type='text' name='mq_host' value='" + escapeHtml(mqtt_host) + "' placeholder='Брокер (host)' autocomplete='url'>";
  html += "<label class='field-label' for='mqtt-port'>Порт MQTT</label><input id='mqtt-port' type='number' name='mq_port' value='" + String(mqtt_port) + "' placeholder='Порт' inputmode='numeric'>";
  html += "<input type='submit' value='💾 Сохранить и перезагрузить' class='btn' style='background:#28a745; width:100%'>";
  html += "</form>";
  html += "</div>";

  // Блок Телеграм
  html += "<div class='card'><h3>🤖 Оповещения Telegram</h3>";
  html += "<form action='/saveTG' method='POST'>";
  html += "<label class='field-label' for='tg-token'>Токен Telegram-бота</label><p class='field-hint' id='tg-token-hint'>Токен " + String(BOT_TOKEN.length() ? "сохранён" : "не задан") + ". Оставьте поле пустым, чтобы не менять его.</p>";
  html += "<input id='tg-token' type='password' name='tg_token' value='' placeholder='Новый токен (необязательно)' autocomplete='new-password' aria-describedby='tg-token-hint'>";
  html += "<label class='field-label' for='tg-chat'>ID чата Telegram</label><p class='field-hint' id='tg-chat-hint'>ID " + String(CHAT_ID.length() ? "сохранён" : "не задан") + ". Оставьте поле пустым, чтобы не менять его. Чтобы узнать ID, найдите бота @myidbot или @userinfobot и отправьте /start.</p>";
  html += "<input id='tg-chat' type='text' name='tg_chat' value='' placeholder='Новый ID чата (необязательно)' autocomplete='off' aria-describedby='tg-chat-hint'>";
  html += "<input type='submit' value='💾 Сохранить и перезагрузить' class='btn' style='background:#28a745; width:100%'>";
  html += "</form></div>";
  
  html += "</body></html>";

  // JavaScript для обработки уровней сигнала
  html += R"rawliteral(<script>

    function updateStatus() {
      fetch('/api/status').then(r => r.json()).then(d => {
        const bars = document.querySelectorAll('.status-card .bar');
        const ssidEl = document.getElementById('ssid-name');
        const ipEl = document.getElementById('sta-ip');
        
        if (d.wifi_conn) {
          if (ssidEl) ssidEl.innerText = d.ssid;
          if (ipEl) ipEl.innerText = d.ip;
          let count = 0, colorClass = '';
          if (d.rssi >= -60) { count = 4; colorClass = 'green'; }
          else if (d.rssi >= -75) { count = 3; colorClass = 'green'; }
          else if (d.rssi >= -85) { count = 2; colorClass = 'yellow'; }
          else { count = 1; colorClass = 'red'; }
          
          bars.forEach((b, i) => {
            b.classList.remove('green', 'yellow', 'red');
            if (i < count) b.classList.add(colorClass);
          });
        } else {
          if (ssidEl) ssidEl.innerHTML = '<span style="color:#dc3545">Отключено</span>';
          if (ipEl) ipEl.innerText = "не присвоен";
          bars.forEach(b => b.classList.remove('green', 'yellow', 'red'));
        }

        const dot = document.getElementById('mqtt-dot');
        const stat = document.getElementById('mqtt-stat');
        if (dot && stat) {
          if (d.mqtt_conn) {
            dot.className = 'dot online';
            stat.innerText = 'В сети';
          } else {
            dot.className = 'dot offline';
            stat.innerText = 'Оффлайн';
          }
        }
      }).catch(err => console.error("Ошибка API статуса:", err));
    }

    function getSignalIcon(dbm) {
      if (dbm >= -50) return '🟢 (Отличный)';
      if (dbm >= -70) return '🟡 (Средний)';
      if (dbm >= -85) return '🟠 (Слабый)';
      return '🔴 (Очень слабый)';
    }

    function scan(btn){
      btn.disabled = true;
      const sel = document.getElementById('netList');
      sel.innerHTML = '<option>Ищу сети...</option>';
      
      fetch('/api/scan').then(r => r.json()).then(data => {
        sel.innerHTML = '<option value="">-- Нажмите для выбора --</option>';
        // Сортировка по уровню сигнала (сначала сильные)
        data.sort((a, b) => b.rssi - a.rssi);
        
        data.forEach(n => {
          let opt = document.createElement('option');
          opt.value = n.ssid;
          opt.text = `${n.ssid} ${getSignalIcon(n.rssi)} [${n.rssi} dBm]`;
          sel.appendChild(opt);
        });
        btn.disabled = false;
      }).catch(() => {
        sel.innerHTML = '<option>Ошибка сканирования</option>';
        btn.disabled = false;
      });
    }

    function togglePass() {
      const p = document.getElementById('passInput');
      const e = document.getElementById('eyeIcon');
      if (p.type === 'password') {
        p.type = 'text';
        e.innerText = '🔒';
      } else {
        p.type = 'password';
        e.innerText = '👁️';
      }
    }

    window.onload = () => scan(document.querySelector('button[onclick^="scan"]'));
    updateStatus();
    setInterval(updateStatus, 1000);
  </script>)rawliteral";
  
  webServer.send(200, "text/html", html);
}

// 6. Страница OTA
void handleOtaPage() {
  if (!isCurrentRequestAdmin()) {
    webServer.sendHeader("Location", "/admin?next=/update", true);
    webServer.send(303, "text/plain", "Сначала включите расширенный режим");
    return;
  }
  String html = getHeader("OTA Обновление прошивки");
  
  html += "<div class='card' style='margin-top: 10px;'>";
  html += "  <h3>🔄 Обновление программного обеспечения</h3>";
  html += "  <p style='font-size: 14px; color: #555; text-align: center; margin-bottom: 20px;'>";
  html += "    Выберите скомпилированный файл <b>firmware.bin</b>.";
  html += "  </p>";
  html += "  <form method='POST' action='/update_action' enctype='multipart/form-data'>";
  html += "    <label class='field-label' for='firmware-file'>Файл прошивки (.bin)</label><input id='firmware-file' type='file' name='update' accept='.bin' style='border: 1px dashed #bbb; padding: 15px; background: #fafafa;' required><br><br>";
  html += "    <input type='submit' class='btn' value='🚀 Начать обновление ПО' style='background: #28a745; width: 100%; margin: 0;'>";
  html += "  </form>";
  html += "</div>";
  html += "</div></body></html>";
  
  webServer.send(200, "text/html", html);
}
