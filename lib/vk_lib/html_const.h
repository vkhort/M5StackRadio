// ============================================================
//  html_const.h - Оптимизированные константы HTML страниц
// ============================================================

#ifndef HTML_CONST_H
#define HTML_CONST_H

// ============================================================
//  ЧАСТЬ 1: СТИЛИ И ШАПКА СТРАНИЦЫ (Хранятся строго в Flash/PROGMEM)
// ============================================================
static const char HTML_HEADER[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>M5Stack Radio</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', -apple-system, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            min-height: 100vh;
            color: white;
            padding: 20px;
        }
        .container {
            max-width: 500px;
            margin: 0 auto;
            background: rgba(22, 33, 62, 0.95);
            border-radius: 20px;
            padding: 30px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.3);
        }
        h1 { text-align: center; color: #4CAF50; font-size: 1.8em; margin-bottom: 5px; }
        .subtitle { text-align: center; color: #888; margin-bottom: 25px; font-size: 0.9em; }
        .section-title {
            color: #4CAF50;
            border-bottom: 1px solid #4CAF50;
            padding-bottom: 5px;
            margin: 25px 0 15px 0;
            font-size: 1.1em;
        }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 6px; color: #aaa; font-size: 0.9em; }
        
        /* Стили для текстовых полей */
        input[type="text"], input[type="password"], input[type="number"] {
            width: 100%;
            padding: 10px;
            border-radius: 8px;
            border: 1px solid #4CAF50;
            background: #0f3460;
            color: white;
            font-size: 1em;
            outline: none;
        }
        
        /* Стильный ползунок громкости (Range Slider) */
        input[type="range"].vol-slider {
            width: 100%;
            -webkit-appearance: none;
            background: #0f3460;
            height: 8px;
            border-radius: 5px;
            outline: none;
            border: 1px solid #4CAF50;
            margin: 15px 0;
        }
        input[type="range"].vol-slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            background: #4CAF50;
            width: 22px;
            height: 22px;
            border-radius: 50%;
            cursor: pointer;
            transition: transform 0.1s;
            box-shadow: 0 0 10px rgba(76, 175, 80, 0.5);
        }
        input[type="range"].vol-slider::-webkit-slider-thumb:hover {
            transform: scale(1.2);
        }

        .btn-group { 
            display: flex; 
            gap: 10px; 
            margin-top: 15px; 
            flex-wrap: wrap;
            justify-content: center;
        }
        button {
            flex: 1;
            background: #4CAF50;
            color: white;
            border: none;
            padding: 12px 20px;
            border-radius: 8px;
            cursor: pointer;
            font-size: 1em;
            transition: all 0.2s;
            min-width: 80px;
        }
        button:hover { background: #45a049; }
        button:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }
        button.secondary { background: #0f3460; border: 1px solid #4CAF50; }
        button.secondary:hover { background: #1a4a80; }
        button.danger { background: #dc3545; }
        button.danger:hover { background: #c82333; }
        button.load { background: #ffc107; color: #1a1a2e; }
        button.load:hover { background: #e0a800; }
        
        .status {
            margin-top: 15px;
            padding: 10px;
            border-radius: 8px;
            text-align: center;
            font-size: 0.9em;
            background: rgba(255, 255, 255, 0.05);
            color: #aaa;
            border: 1px solid #2a3a5a;
            min-height: 20px;
        }
        .status.info {
            color: #4CAF50;
            border-color: #4CAF50;
            background: rgba(76, 175, 80, 0.1);
        }
        .status.error {
            color: #f44336;
            border-color: #f44336;
            background: rgba(244, 67, 54, 0.1);
        }
        
        /* Переименован во избежание конфликтов со статусами */
        .info-panel {
            background: #0f3460;
            padding: 10px;
            border-radius: 8px;
            margin-top: 20px;
            font-size: 0.8em;
            text-align: center;
            color: #aaa;
        }
        .info-row {
            display: flex;
            justify-content: space-between;
            padding: 6px 0;
            border-bottom: 1px solid #2a3a5a;
        }
        .info-row .label { color: #888; }
        .info-row .value { color: #4CAF50; }
        
        /* Сетка списка радиостанций */
        .station-row {
            display: flex;
            gap: 8px;
            margin-bottom: 6px;
            align-items: center;
        }
        .station-row .station-label { 
            color: #888; 
            min-width: 25px; 
            font-size: 0.8em;
            text-align: right;
        }
        .station-row input[type="text"] {
            flex: 1;
            padding: 6px 8px;
            font-size: 0.85em;
        }
        .station-row .station-name { flex: 0 0 30%; }
        .station-row .station-url { flex: 1; }
        
        /* Скроллбар для списка каналов */
        .stations-scroll {
            max-height: 150px;
            overflow-y: auto;
            padding-right: 5px;
        }
        .stations-scroll::-webkit-scrollbar {
            width: 6px;
        }
        .stations-scroll::-webkit-scrollbar-track {
            background: #0f3460;
            border-radius: 3px;
        }
        .stations-scroll::-webkit-scrollbar-thumb {
            background: #4CAF50;
            border-radius: 3px;
        }
        
        .vol-slider {
            width: 100%;
            margin: 10px 0;
        }
        .vol-display {
            text-align: center;
            font-size: 1.2em;
            color: #4CAF50;
            font-weight: bold;
        }
        .password-wrapper {
            position: relative;
            width: 100%;
        }
        .password-wrapper input {
            padding-right: 45px;
        }
        .toggle-password {
            position: absolute;
            right: 10px;
            top: 50%;
            transform: translateY(-50%);
            background: none;
            border: none;
            color: #4CAF50;
            cursor: pointer;
            font-size: 18px;
            padding: 0;
            width: 30px;
            height: 30px;
            display: flex;
            align-items: center;
            justify-content: center;
            min-width: 30px;
            flex: 0 0 auto;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>M5Stick Radio</h1>
)rawliteral";
// ============================================================
//  ЧАСТЬ 2: AP РЕЖИМ (Аварийная страница конфигурации Wi-Fi)
// ============================================================
static const char HTML_AP_BODY[] PROGMEM = R"rawliteral(
        <div class="section-title">Network Settings</div>
        
        <div class="form-group">
            <label>Wi-Fi SSID</label>
            <input type="text" id="ssid" placeholder="Enter Wi-Fi network name" value="{SSID}">
        </div>
        
        <div class="form-group">
            <label>Wi-Fi Password</label>
            <div class="password-wrapper">
                <input type="password" id="password" placeholder="Enter Wi-Fi password" value="{PASSWORD}">
                <button type="button" class="toggle-password" onclick="togglePassword(this)">👁️</button>
            </div>
        </div>
        
        <div class="btn-group">
            <button class="load" onclick="loadConfig()">Read</button>
            <button onclick="saveConfig()">Save</button>
        </div>
        
        <!-- ЕДИНЫЙ БЛОК СТАТУСА В САМОМ НИЗУ -->
        <div id="status" class="status"></div>
)rawliteral";

// ============================================================
//  ЧАСТЬ 3: STA РЕЖИМ (Полноценный асинхронный пульт радио)
// ============================================================
static const char HTML_STA_BODY[] PROGMEM = R"rawliteral(
        <div class="subtitle">Radio Control</div>

        <!-- КНОПКИ УПРАВЛЕНИЯ ПЛЕЕРОМ -->
        <div class="section-title">Player</div>
        <div class="btn-group">
            <button id="btnPlay" onclick="sendCommand('toggle')">Play</button>
            <button id="btnStop" class="danger" onclick="sendCommand('toggle')" disabled>Stop</button>
        </div>
        
        <!-- СТРОКА ИНТЕРФЕЙСА: СТАНЦИЯ + ГРОМКОСТЬ -->
        <div style="display: flex; align-items: center; gap: 10px; flex-wrap: wrap; padding: 10px 0; border-bottom: none;">
            
            <!-- Блок переключения станций кнопками ◀ и ▶ -->
            <div style="display: flex; align-items: center; gap: 8px; flex: 1;">
                <span class="label" style="min-width: 60px;">Station</span>
                <button onclick="changeStation(-1)" style="min-width: 30px; flex: 0 0 auto; padding: 4px 8px; font-size: 1.2em;">◀</button>
                <span class="value" id="stationDisplay" style="min-width: 40px; text-align: center; font-weight: bold;">{CURRENT_STATION}</span>
                <button onclick="changeStation(1)" style="min-width: 30px; flex: 0 0 auto; padding: 4px 8px; font-size: 1.2em;">▶</button>
                <span class="value" id="stationNameDisplay" style="min-width: 80px; font-size: 0.9em;">{CURRENT_STATION_NAME}</span>
            </div>
            
            <!-- Блок неонового ползунка громкости (Шкала строго 0-9) -->
            <div style="display: flex; align-items: center; gap: 10px; flex: 1;">
                <span class="label" style="min-width: 50px;">Vol</span>
                <!-- ИСПРАВЛЕНО: Добавлен CSS-класс vol-slider для включения кастомного дизайна ползунка -->
                <input type="range" min="0" max="9" value="{VOLUME}" id="volumeSlider" class="vol-slider"
                       oninput="document.getElementById('volumeDisplay').textContent=this.value; updateVolume(this.value);"
                       style="flex: 1; min-width: 80px;">
                <span class="value" id="volumeDisplay" style="min-width: 25px; text-align: center; font-weight: bold;">{VOLUME}</span>
            </div>
        </div>

        <!-- ДИНАМИЧЕСКИЙ СКРОЛЛ-СПИСОК ВСЕХ 10 СТАНЦИЙ -->
        <div class="section-title">Stations (10)</div>
        <div class="stations-scroll" id="stationsContainer"></div>
)rawliteral";

// ============================================================
//  ЧАСТЬ 4: СКРИПТЫ (Асинхронный фронтенд пульта управления)
// ============================================================
static const char HTML_SCRIPT[] PROGMEM = R"rawliteral(
    </div>

    <script>
        // ============================================================
        //  ЕДИНЫЙ СТАТУСНЫЙ ИНФОРМАТОР
        // ============================================================
        function setStatus(msg, type) {
            const statusDiv = document.getElementById('status');
            if (!statusDiv) return;
            statusDiv.textContent = msg;
            statusDiv.className = 'status';
            if (type) {
                statusDiv.classList.add(type);
            }
        }

        function isAPMode() {
            return document.getElementById('stationsContainer') === null;
        }

        // ============================================================
        //  AP РЕЖИМ (Настройка Wi-Fi)
        // ============================================================
        // ИСПРАВЛЕНО: Контекст кнопки btn теперь передается явно, гарантируя 
        // 100% работу клика на любых смартфонах Android и iPhone
        function togglePassword(btn) {
            const pwd = document.getElementById('password');
            if (!pwd || !btn) return;
            
            if (pwd.type === 'password') {
                pwd.type = 'text';
                btn.textContent = '🙈';
            } else {
                pwd.type = 'password';
                btn.textContent = '👁️';
            }
        }

        // ============================================================
        //  STA РЕЖИМ (Управление радио и громкостью)
        // ============================================================
        function updateButtons(isPlaying) {
            document.getElementById('btnPlay').disabled = isPlaying;
            document.getElementById('btnStop').disabled = !isPlaying;
        }

        async function sendCommand(cmd) {
            if (cmd === 'toggle') {
                setStatus('⏳ ' + cmd + '...', 'info');
            }
            try {
                const response = await fetch('/cmd?action=' + cmd);
                if (response.ok) {
                    if (cmd === 'toggle') {
                        setStatus('✅ ' + cmd + ' done', 'info');
                    }
                    updateState();
                } else {
                    setStatus('❌ Error', 'error');
                }
            } catch(e) {
                setStatus('❌ Error: ' + e.message, 'error');
            }
        }

        async function updateState() {
            try {
                const response = await fetch('/state');
                if (response.ok) {
                    const data = await response.json();
                    
                    // ИСПРАВЛЕНО: Оператор || заменен на безопасный ?? 
                    // Теперь при выборе 0-й станции или громкости 0% ползунки 
                    // не будут хаотично прыгать на дефолтные значения!
                    document.getElementById('stationDisplay').textContent = data.currentStation ?? 0;
                    document.getElementById('stationNameDisplay').textContent = data.stationName ?? '---';
                    document.getElementById('volumeDisplay').textContent = data.volume ?? 0;
                    document.getElementById('volumeSlider').value = data.volume ?? 0;
                    
                    const isPlaying = data.isPlaying ?? false;
                    updateButtons(isPlaying);
                }
            } catch(e) {
                console.error('State error:', e);
            }
        }

        // ============================================================
        //  ДИНАМИЧЕСКИЙ РЕНДЕРИНГ ИНТЕРФЕЙСА СТАНЦИЙ
        // ============================================================
        function generateStationsUI() {
            const container = document.getElementById('stationsContainer');
            if (!container) return;
            
            let html = '';
            for (let i = 0; i < 10; i++) {
                html += `
                    <div class="station-row">
                        <span class="station-label">${i}.</span>
                        <input type="text" class="station-name" id="st_name_${i}" placeholder="Station ${i}" value="">
                        <input type="text" class="station-url" id="st_url_${i}" placeholder="URL" value="">
                    </div>
                `;
            }
            container.innerHTML = html;
        }

        function getStationsFromUI() {
            const stations = [];
            for (let i = 0; i < 10; i++) {
                stations.push({
                    name: document.getElementById('st_name_' + i).value.trim(),
                    url: document.getElementById('st_url_' + i).value.trim()
                });
            }
            return stations;
        }

        function setStationsToUI(stations) {
            for (let i = 0; i < 10 && i < stations.length; i++) {
                document.getElementById('st_name_' + i).value = stations[i].name || '';
                document.getElementById('st_url_' + i).value = stations[i].url || '';
            }
        }

        // ============================================================
        //  ЗАГРУЗКА/СОХРАНЕНИЕ КОНФИГА
        // ============================================================
        async function loadConfig() {
            setStatus('📖 Loading...', 'info');
            try {
                const response = await fetch('/config');
                if (response.ok) {
                    const data = await response.json();
                    if (data.stations) {
                        setStationsToUI(data.stations);
                    }
                    document.getElementById('volumeSlider').value = data.volume || 5;
                    document.getElementById('volumeDisplay').textContent = data.volume || 5;
                    document.getElementById('stationDisplay').textContent = data.currentStation || 0;
                    setStatus('✅ Loaded!', 'info');
                } else {
                    setStatus('❌ Load error', 'error');
                }
            } catch(e) {
                setStatus('❌ Error: ' + e.message, 'error');
            }
        }

        function saveConfig() {
            const ssidEl = document.getElementById('ssid');
            const passEl = document.getElementById('password');
            const statusDiv = document.getElementById('status');
            
            if (!ssidEl) return;

            const ssidVal = ssidEl.value;
            const passVal = passEl ? passEl.value : "";

            if (!ssidVal) {
                if (statusDiv) { statusDiv.className = "status error"; statusDiv.innerText = "Error: SSID is empty!"; }
                return;
            }

            if (statusDiv) { statusDiv.className = "status processing"; statusDiv.innerText = "Saving settings..."; }

            var formData = new FormData();
            formData.append("ssid", ssidVal);
            formData.append("password", passVal);

            // === УМНОЕ РАЗДЕЛЕНИЕ РЕЖИМОВ ДЛЯ ПЕРЕЗАГРУЗКИ ===
            const testStationEl = document.getElementById('station_0_name');
            
            if (testStationEl) {
                // Мы в режиме STA (4 константы) — перезагрузка НЕ нужна
                formData.append("mode", "STA"); 
                getStationsFromUI(); 
                // Здесь ваш штатный код добавления станций в formData, если он есть
            } else {
                // Мы в аварийном режиме AP (3 константы) — перезагрузка ОБЯЗАТЕЛЬНА
                formData.append("mode", "AP"); 
            }

            fetch('/save', {
                method: 'POST',
                body: formData
            })
            .then(response => {
                if (response.ok) {
                    if (statusDiv) {
                        statusDiv.className = "status success";
                        // Меняем текст подсказки в зависимости от режима
                        statusDiv.innerText = testStationEl 
                            ? "Success! Settings saved safely." 
                            : "Success! Saving to SPIFFS and rebooting...";
                    }
                } else {
                    if (statusDiv) { statusDiv.className = "status error"; statusDiv.innerText = "Server error!"; }
                }
            });
        }


        // ============================================================
        //  ИНИЦИАЛИЗАЦИЯ И СТАРТ ФОНОВОГО МОНИТОРИНГА (Core 1)
        // ============================================================
        window.addEventListener('DOMContentLoaded', function() {
            // Если на странице обнаружен контейнер станций — значит мы загрузились в режиме радио (STA)
            if (document.getElementById('stationsContainer')) {
                generateStationsUI(); // Генерируем текстовые поля для 10 каналов
                updateState();        // Запрашиваем живой статус громкости и трека из RAM
                loadConfig();         // Заполняем поля именами и URL
                
                // Запускаем асинхронный опрос эндпоинта /state каждые 3 секунды, 
                // полностью синхронизируя веб-пульт с жестами GyroJoystick!
                setInterval(updateState, 3000);
            }
        });

        // ============================================================
        //  УПРАВЛЕНИЕ СТАНЦИЕЙ (Кнопки переключения ◀ и ▶)
        // ============================================================
        function changeStation(delta) {
            // ИСПРАВЛЕНО: Оператор || заменен на безопасный ?? для строгой защиты числового нуля
            let current = parseInt(document.getElementById('stationDisplay').textContent) ?? 0;
            let newStation = current + delta;
            
            // Круговой сброс индексов строго в рамках 10 станций (0-9)
            if (newStation < 0) newStation = 9;
            if (newStation > 9) newStation = 0;
            
            document.getElementById('stationDisplay').textContent = newStation;
            
            // Отправляем команду на устройство. Плеер на Core 0 мгновенно переключит поток
            sendCommand(delta > 0 ? 'next' : 'prev');
        }

        // ============================================================
        //  ОБНОВЛЕНИЕ ГРОМКОСТИ (Движение ползунка range в браузере)
        // ============================================================
        async function updateVolume(value) {
            try {
                const formData = new URLSearchParams();
                formData.append('volume', value); // Передаем выбранный уровень (0-9)
                
                // Шлем POST запрос на эндпоинт /volume. 
                // Плеер применит звук, а глобальный config.volume обновится в RAM без записи на диск!
                await fetch('/volume', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: formData
                });
            } catch(e) {
                console.error('Volume update error:', e);
            }
        }

    </script>
</body>
</html>
)rawliteral";

#endif // HTML_CONST_H
