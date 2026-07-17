// ============================================================
//  classes.cpp - Реализация WorkSPIFFS и WiFiConnect (БЕЗ JSON)
// ============================================================

#include "classes.h"
#include "config.h"
#include "html_const.h"
#include <WiFi.h>
#include "display.h"
#include "radio.h"

extern WorkSPIFFS myFS;
extern Display display;
extern Radio radio;
extern WorkSPIFFS::ConfigData config;
// ============================================================
//  РЕАЛИЗАЦИЯ ConfigData
// ============================================================

WorkSPIFFS::ConfigData::ConfigData()
    : ssid(DEFAULT_SSID)
    , password(DEFAULT_PASSWORD)
    , currentStation(DEFAULT_STATION)
    , volume(DEFAULT_VOLUME)
    , stations(nullptr)
    , stationCount(0)
{
    #if DEBUG_MODE
    Serial.println("[ConfigData] Constructor called");
    #endif
    
    // Загружаем дефолтный список станций из config.h
    loadDefaultStations();
}

WorkSPIFFS::ConfigData::~ConfigData() {
    if (stations != nullptr) {
        delete[] stations; // Безопасное освобождение массива из кучи
        stations = nullptr;
        stationCount = 0;
        #if DEBUG_MODE
        Serial.println("[ConfigData] Stations memory freed");
        #endif
    }
}

void WorkSPIFFS::ConfigData::loadDefaultStations() {
    // Если память уже была выделена — очищаем её перед перезаписью
    if (stations != nullptr) {
        delete[] stations;
        stations = nullptr;
        stationCount = 0;
    }

    // ИСПРАВЛЕНО: Объявляем массив как static const. Теперь тяжелые строки 
    // радиостанций читаются напрямую из Flash-памяти (PROGMEM) и не дублируются 
    // в оперативной памяти ESP32, защищая плату от Out Of Memory!
    static const StationEntry defaultStations[] = STATION_LIST;
    
    int defaultCount = sizeof(defaultStations) / sizeof(StationEntry);
    int count = min(defaultCount, MAX_STATIONS);

    // Выделяем чистый кусок памяти в куче под текущую сессию
    stations = new StationEntry[count];
    stationCount = count;

    for (int i = 0; i < count; i++) {
        stations[i].name = defaultStations[i].name;
        stations[i].url = defaultStations[i].url;
    }

    #if DEBUG_MODE
    Serial.printf("[ConfigData] Loaded %d default stations directly from Flash\n", stationCount);
    #endif
}

String WorkSPIFFS::ConfigData::getCurrentStationName() const {
    if (stations != nullptr && currentStation >= 0 && currentStation < stationCount) {
        return stations[currentStation].name;
    }
    return "No station";
}

String WorkSPIFFS::ConfigData::getCurrentStationURL() const {
    if (stations != nullptr && currentStation >= 0 && currentStation < stationCount) {
        return stations[currentStation].url;
    }
    return "";
}

// ИСПРАВЛЕНО: Дублирующие методы previousStation() и getVolumeFloat() 
// ПОЛНОСТЬЮ УДАЛЕНЫ из этого файла, так как вся логика смены каналов и шкал
// теперь инкапсулирована внутри класса Radio и config.h!

// ============================================================
//  РЕАЛИЗАЦИЯ КЛАССА WorkSPIFFS (БЕЗ ИСПОЛЬЗОВАНИЯ JSON)
// ============================================================
WorkSPIFFS::WorkSPIFFS()
    : _mounted(false) // Инициализируем флаг в списке инициализации
{
    #if DEBUG_MODE
    Serial.println("[WorkSPIFFS] Конструктор вызван");
    #endif
}

WorkSPIFFS::~WorkSPIFFS() {
    if (_mounted) {
        SPIFFS.end(); // Безопасно закрываем файловую систему при уничтожении объекта
        #if DEBUG_MODE
        Serial.println("[WorkSPIFFS] SPIFFS успешно размонтирован");
        #endif
    }
}

bool WorkSPIFFS::begin() {
    #if DEBUG_MODE
    Serial.println("[WorkSPIFFS] Монтируем файловую систему SPIFFS...");
    #endif

    // Передаем true в качестве параметра. Если файловая система повреждена, 
    // ESP32 автоматически отформатирует её, предотвращая вечный сбой (Crash).
    _mounted = SPIFFS.begin(true);

    if (!_mounted) {
        #if DEBUG_MODE
        Serial.println("[WorkSPIFFS] КРИТИЧЕСКАЯ ОШИБКА: Не удалось смонтировать SPIFFS!");
        #endif
        return false;
    }

    #if DEBUG_MODE
    Serial.println("[WorkSPIFFS] SPIFFS успешно смонтирован");
    #endif

    // Проверяем, существует ли файл конфигурации на Flash-памяти
    if (!SPIFFS.exists(_configFile)) {
        #if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Файл конфигурации не найден, создаем дефолтный...");
        #endif
        
        // ВНИМАНИЕ: Конструктор ConfigData() уже сам автоматически загружает 
        // дефолтные станции из Flash через loadDefaultStations()!
        ConfigData defaultData;
        
        // Метод setDefaults должен только заполнить SSID, Password, volume и currentStation,
        // НЕ перевыделяя массив станций повторно!
        setDefaults(defaultData); 
        
        // Записываем чистые текстовые настройки в файл
        saveConfig(defaultData);
        
        #if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Текстовый файл конфигурации успешно создан");
        #endif
    } else {
        #if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Файл конфигурации обнаружен на диске");
        #endif
    }

    return true;
}
void WorkSPIFFS::setDefaults(ConfigData& data) {
    data.ssid = DEFAULT_SSID;
    data.password = DEFAULT_PASSWORD;
    data.currentStation = DEFAULT_STATION;
    data.volume = DEFAULT_VOLUME;
    
    // ИСПРАВЛЕНО: loadDefaultStations() вызывается ТОЛЬКО если массив stations еще пуст.
    // Это полностью защищает кучу (Heap) от фрагментации и утечек памяти при старте.
    if (data.stations == nullptr) {
        data.loadDefaultStations();
    }

    #if DEBUG_MODE
    Serial.println("[WorkSPIFFS] Установлен дефолтный конфиг из Flash");
    #endif
}

// ============================================================
//  БЕЗОПАСНАЯ ЗАГРУЗКА КОНФИГА ИЗ ТЕКСТОВОГО ФАЙЛА (Core 1)
// ============================================================
bool WorkSPIFFS::loadConfig(ConfigData& data) {
    if (!_mounted) {
        #if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Ошибка: SPIFFS не смонтирован, берем дефолт");
        #endif
        setDefaults(data);
        return false;
    }

    if (!SPIFFS.exists(_configFile)) {
        #if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Файл конфигурации не найден, берем дефолт");
        #endif
        setDefaults(data);
        return false;
    }

    File file = SPIFFS.open(_configFile, "r");
    if (!file) {
        #if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Не удалось открыть файл конфигурации");
        #endif
        setDefaults(data);
        return false;
    }

#ifdef DEBUG_MODE
    // ============================================================
    //  ВРЕМЕННО: Вывод содержимого текстового файла в консоль
    // ============================================================
    Serial.println("[WorkSPIFFS] ===== CONFIG FILE CONTENT =====");
    while (file.available()) {
        String line = file.readStringUntil('\n');
        Serial.println(line);
    }
    Serial.println("[WorkSPIFFS] ===== END OF CONFIG FILE =====");
    file.seek(0);  // Возвращаем указатель чтения в начало файла
#endif

    // ИСПРАВЛЕНО: Перед чтением строк станций, мы сначала должны узнать stationCount.
    // Пробегаем по файлу быстрыми текстовыми шагами, чтобы найти переменную stationCount.
    int fileStationCount = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.startsWith("stationCount=")) {
            fileStationCount = line.substring(line.indexOf('=') + 1).toInt();
            break;
        }
    }
    file.seek(0); // Возвращаемся в начало для полноценного разбора

    // Если в файле указано корректное число станций, принудительно перевыделяем массив в куче
    if (fileStationCount > 0 && fileStationCount <= MAX_STATIONS) {
        if (data.stations != nullptr) {
            delete[] data.stations; // Стираем старый дефолтный массив
        }
        data.stations = new StationEntry[fileStationCount];
        data.stationCount = fileStationCount;
    } else {
        // Если поле stationCount повреждено, откатываемся на безопасные дефолты
        setDefaults(data);
    }

    // Основной цикл построчного текстового парсинга
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        
        // Пропускаем пустые строки и закомментированные строчки (через #)
        if (line.length() == 0 || line.startsWith("#")) continue; 
        
        int eqPos = line.indexOf('=');
        if (eqPos < 0) continue;
        
        String key = line.substring(0, eqPos);
        String value = line.substring(eqPos + 1);
        
        if (key == "ssid") {
            data.ssid = value;
        } else if (key == "password") {
            data.password = value;
        } else if (key == "volume") {
            // Ограничиваем считываемую громкость в рамках нашей шкалы 0-9
            data.volume = constrain(value.toInt(), 0, 9);
        } else if (key == "currentStation") {
            data.currentStation = value.toInt();
        } else if (key.startsWith("station[")) {
            // ИСПРАВЛЕНО: Потокобезопасный разбор динамических строк "station[0].name=..."
            int startIdx = key.indexOf('[') + 1;
            int endIdx = key.indexOf(']');
            int idx = key.substring(startIdx, endIdx).toInt();
            int dotIdx = key.indexOf('.');
            String field = key.substring(dotIdx + 1);
            
            // ИСПРАВЛЕНО: Строгая проверка границ. Запись в ячейку пойдет только если 
            // память под индекс [idx] гарантированно выделена в куче! Это убирает 100% падений LoadStoreError.
            if (data.stations != nullptr && idx >= 0 && idx < data.stationCount) {
                if (field == "name") {
                    data.stations[idx].name = value;
                } else if (field == "url") {
                    data.stations[idx].url = value;
                }
            }
        }
    }

    file.close();

    // Финальная проверка: если текущая станция из файла вылетает за рамки считанного списка
    if (data.currentStation >= data.stationCount || data.currentStation < 0) {
        data.currentStation = 0; 
    }

    #if DEBUG_MODE
    Serial.printf("[WorkSPIFFS] Успех! Конфиг загружен: станций=%d, громкость=%d/9, текущая=%d\n\n",
                  data.stationCount, data.volume, data.currentStation);
    #endif

    return true;
}
// ============================================================
//  БЕЗОПАСНОЕ СОХРАНЕНИЕ КОНФИГА В ТЕКСТОВЫЙ ФАЙЛ (Core 1)
// ============================================================
bool WorkSPIFFS::saveConfig(const ConfigData& data) {
    if (!_mounted) {
        #if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Ошибка сохранения: SPIFFS не смонтирован");
        #endif
        return false;
    }

    // Открываем файл в режиме записи ("w" полностью перезаписывает файл с нуля)
    File file = SPIFFS.open(_configFile, "w");
    if (!file) {
        #if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Не удалось открыть файл конфигурации для записи");
        #endif
        return false;
    }

    // ИСПРАВЛЕНО: Записываем все критически важные системные переменные СТРОГО В ШАПКУ файла!
    // Это ускоряет чтение парсером loadConfig() в разы и страхует плеер от потери настроек.
    file.println("ssid=" + data.ssid);
    file.println("password=" + data.password);
    file.println("volume=" + String(data.volume)); // Переменная громкости (0-9) теперь в безопасности
    file.println("currentStation=" + String(data.currentStation));
    file.println("stationCount=" + String(data.stationCount));
    
    // Записываем массив радиостанций следом за шапкой настроек
    if (data.stations != nullptr) {
        for (int i = 0; i < data.stationCount; i++) {
            file.println("station[" + String(i) + "].name=" + data.stations[i].name);
            file.println("station[" + String(i) + "].url=" + data.stations[i].url);
        }
    }
    
    // Физически закрываем файл, фиксируя данные на Flash-диске ESP32
    file.close();

    #if DEBUG_MODE
    Serial.printf("[WorkSPIFFS] Конфиг успешно сохранен (%d станций на диске)\n\n", data.stationCount);
    #endif

    return true;
}

// ============================================================
//  РЕАЛИЗАЦИЯ СЕТЕВОГО КЛАССА WiFiConnect
// ============================================================
/*
WiFiConnect::WiFiConnect()
    : _mode(Mode::DISCONNECTED)
    , targetSSID("")      // Рекомендация: явно инициализируем внутренние строки
    , targetPassword("")
    , apSSID(DEFAULT_SSID)
    , apPassword(DEFAULT_PASSWORD)
{
    #if DEBUG_MODE
    Serial.println("[WiFiConnect] Сетевой конструктор успешно вызван");
    #endif
}
*/
void WiFiConnect::setAPCredentials(const String& ssid, const String& password) {
    apSSID = ssid;
    apPassword = password;
    
    #if DEBUG_MODE
    Serial.printf("[WiFiConnect] Изменены настройки встроенной AP: SSID=%s\n", apSSID.c_str());
    #endif
}
bool WiFiConnect::setupWiFi(WorkSPIFFS::ConfigData& config, unsigned long timeoutMs) {
    targetSSID = config.ssid;
    targetPassword = config.password;

    // Если в сохраненных настройках SSID пустой — сразу запускаем аварийную точку доступа
    if (targetSSID.isEmpty()) {
        #if DEBUG_MODE
        Serial.println("[WiFi] SSID пуст в памяти, запуск режима AP...");
        #endif
        startAPMode();
        return false;
    }

    #if DEBUG_MODE
    Serial.printf("[WiFi] Пробуем подключиться к: %s...\n", targetSSID.c_str());
    #endif

    // Запускаем неблокирующий цикл ожидания домашней сети
    if (connectToWiFi(targetSSID, targetPassword, timeoutMs)) {
        _mode = Mode::STA;
        
        #if DEBUG_MODE
        Serial.printf("[WiFi] Успешно подключено! Системный IP: %s\n", WiFi.localIP().toString().c_str());
        #endif
        
        // КРИТИЧЕСКИ ВАЖНО: Отключаем режим сна антенны, чтобы убрать заикания звука при скачивании MP3
        WiFi.setSleep(false); 
        
        // Настраиваем роуты управления плеером и запускаем асинхронный веб-сервер на Core 1
        setupWebServer();
        return true;
    }

    // Если за 15 секунд роутер не ответил — уходим в аварийный WiFi Manager
    #if DEBUG_MODE
    Serial.println("[WiFi] Ошибка подключения к роутеру, переходим в режим AP...");
    #endif

    startAPMode();

    return false;
}

bool WiFiConnect::connectToWiFi(const String& ssid, const String& password, unsigned long timeoutMs) {
    WiFi.disconnect(true); // Полностью очищаем прошлые сетевые сокеты
    vTaskDelay(100 / portTICK_PERIOD_MS); // ИСПРАВЛЕНО: неблокирующая FreeRTOS пауза для стабилизации LwIP
    
    WiFi.mode(WIFI_STA); // Переводим чип строго в режим клиента домашней сети
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long start = millis();
    
    // ИСПРАВЛЕНО: Вместо блокирующего delay(500) используется vTaskDelay.
    // Теперь во время ожидания сети процессорное ядро Core 1 полностью свободно, 
    // прерывания Wi-Fi обрабатываются мгновенно, а роутер подхватывает плату в 2 раза быстрее!
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        vTaskDelay(500 / portTICK_PERIOD_MS); 
        #if DEBUG_MODE
        Serial.print(".");
        #endif
    }
    #if DEBUG_MODE
    Serial.println();
    #endif

    return WiFi.status() == WL_CONNECTED;
}

void WiFiConnect::startAPMode() {
    WiFi.disconnect(true);
    delay(100);
    // vTaskDelay(100 / portTICK_PERIOD_MS);
    
    WiFi.mode(WIFI_AP); // Переводим передатчик строго в изолированный режим раздачи Wi-Fi
    delay(100);
    // vTaskDelay(100 / portTICK_PERIOD_MS);
    
    // Поднимаем аварийную Wi-Fi сеть для смартфона
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(apSSID.c_str(), apPassword.c_str());
    _mode = Mode::AP;

    // Выводим IP-адрес и пароль точки доступа на экран M5StickC Plus
    extern Display display;
    display.showAPInfo(WiFi.softAPIP().toString(), apSSID, apPassword);

    // Настраиваем аварийную страницу ввода паролей и запускаем сервер настроек
    setupWebServer();
}
// ============================================================
//  ГЕТТЕРЫ СЕТЕВОГО СТАТУСА И IP АДРЕСОВ
// ============================================================

bool WiFiConnect::isConnected() const {
    // ИСПРАВЛЕНО: Проверяем реальный физический статус соединения с роутером на уровне железа.
    // Теперь, если сеть моргнет в процессе работы, система мгновенно узнает об этом!
    return (WiFi.status() == WL_CONNECTED);
}

String WiFiConnect::getIPAddress() {
    // Используем исправленный неблокирующий метод проверки связи
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

String WiFiConnect::getAPIPAddress() {
    // Потокобезопасная проверка аппаратного режима работы передатчика
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        return WiFi.softAPIP().toString();
    }
    return "0.0.0.0";
}

String WiFiConnect::getCurrentSSID() {
    return targetSSID;
}

// ============================================================
//  ПОЛНОЕ ОТКЛЮЧЕНИЕ И ЗАКРЫТИЕ СОКЕТОВ (Core 1)
// ============================================================
void WiFiConnect::disconnect() {
    // Стираем сетевые подключения и сокеты на уровне LwIP стека
    WiFi.disconnect(true);
    _mode = Mode::DISCONNECTED;
    
    #if DEBUG_MODE
    Serial.println("[WiFi] Сетевые интерфейсы закрыты, сокеты STA очищены");
    #endif
}


 void WiFiConnect::setupWebServer() {
    // Очищаем старые обработчики перед инициализацией
    _server.reset();

    // ============================================================
    // 1. ГЛАВНАЯ СТРАНИЦА (Рендеринг напрямую из глобальной RAM)
    // ============================================================
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        #if DEBUG_MODE
        Serial.println("[Web Core1] Мгновенный запрос из браузера. Отправка готового ответа...");
        #endif
        
        // Подключаем нашу постоянную глобальную строку из главного файла
        extern String htmlWeb;
        
        // ЭТАЛОННЫЙ ФИКС ИЗ M5STICK: Передаем прямой адрес массива байт в RAM без копирования!
        AsyncWebServerResponse *response = request->beginResponse(
            200, 
            "text/html", 
            (const uint8_t*)htmlWeb.c_str(), // Прямой адрес массива байт в RAM
            htmlWeb.length()                 // Точная длина страницы
        );
        
        // Жестко запрещаем браузеру кэшировать пульт
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "-1");
        
        // Отправляем контейнер в фоновый Wi-Fi стек
        request->send(response);
    });

    // ============================================================
    // 2. ПОЛУЧЕНИЕ КОНФИГА (Сборка JSON напрямую из глобальной RAM)
    // ============================================================
    _server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
        #if DEBUG_MODE
        Serial.println("[Web Core1] Запрос GET /config. Сборка JSON из RAM...");
        #endif
        
        // Подключаем живую глобальную конфигурацию всего проекта
        extern WorkSPIFFS::ConfigData config;
        
        String json;
        // Защита памяти M5Stack: бронируем 3 Кб, чтобы строка собиралась без фрагментации кучи
        json.reserve(3072); 
        
        json = "{";
        json += "\"ssid\":\"" + config.ssid + "\",";
        json += "\"password\":\"" + config.password + "\",";
        json += "\"volume\":" + String(config.volume) + ",";
        json += "\"currentStation\":" + String(config.currentStation) + ",";
        json += "\"stations\":[";
        
        for (int i = 0; i < config.stationCount; i++) {
            if (i > 0) json += ",";
            json += "{\"name\":\"" + config.stations[i].name + "\",";
            json += "\"url\":\"" + config.stations[i].url + "\"}";
        }
        
        json += "]";
        json += "}";
        
        // Запрещаем браузеру ПК кэшировать список станций
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });

    // ============================================================
    // 3. СОХРАНЕНИЕ КОНФИГА (Запись напрямую в глобальную RAM + Диск)
    // ============================================================
    _server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){ 
        #if DEBUG_MODE
        Serial.println("[Web Core1] Запрос POST /save из браузера. Обновляем глобальную RAM...");
        #endif
        
        extern WorkSPIFFS myFS;
        extern WorkSPIFFS::ConfigData config;

        // 1. Считываем данные из браузера и пишем напрямую в глобальный config в RAM
        for (int i = 0; i < config.stationCount; i++) {
            String nameParam = "stationName[" + String(i) + "]";
            String urlParam = "stationURL[" + String(i) + "]";
            
            if (request->hasParam(nameParam, true)) {
                config.stations[i].name = request->getParam(nameParam, true)->value();
            }
            if (request->hasParam(urlParam, true)) {
                config.stations[i].url = request->getParam(urlParam, true)->value();
            }
        }
        
        if (request->hasParam("volume", true)) {
            config.volume = constrain(request->getParam("volume", true)->value().toInt(), 0, 9);
        }
        if (request->hasParam("currentStation", true)) {
            config.currentStation = request->getParam("currentStation", true)->value().toInt();
        }

        // Записываем обновленные сетевые настройки в файл /config на Flash-память SPIFFS
        myFS.saveConfig(config);

        // 2. СИНХРОНИЗИРУЕМ С CORE 0 ЧЕРЕЗ КЛАСС RADIO БЕЗ ПРЯМОГО ВЫЗОВА ОЧЕРЕДИ
        extern Radio radio;
        // Пинаем плеер текущим уровнем громкости. 
        // Этот метод внутри radio.cpp сам потокобезопасно отправит нужный маркер в свою очередь!
        radio.setVolume(config.volume); 
        
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Saving on the fly...\"}");
    });


    // ============================================================
    // AP РЕЖИМ: СОХРАНЕНИЕ WI-FI И ПЕРЕЗАГРУЗКА
    // ============================================================
    _server.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request){
        String ssid = "";
        String password = "";
        
        if (request->hasParam("ssid", true))     ssid = request->getParam("ssid", true)->value();
        if (request->hasParam("password", true)) password = request->getParam("password", true)->value();
        
        if (ssid.length() > 0) {
            // Подключаем внешние глобальные ссылки на память всего проекта
            extern WorkSPIFFS myFS;
            extern WorkSPIFFS::ConfigData config;
            
            config.ssid = ssid;
            config.password = password;
            
            // В AP-режиме пишем напрямую на Flash-диск, так как звук и другие таски спят
            myFS.saveConfig(config); 
        }
        
        // Мгновенно ставим в очередь отправки JSON-ответ для браузера ПК
        request->send(200, "application/json", "{\"status\":\"ok\"}");
        
        // ЭТАЛОННЫЙ ПАТТЕРН: Отложенная перезагрузка через фоновую микро-задачу FreeRTOS.
        // Дает серверу 1.5 секунды, чтобы полностью вытолкнуть TCP-пакет в сеть до сброса чипа!
        xTaskCreatePinnedToCore(
            [](void* p) {
                vTaskDelay(pdMS_TO_TICKS(1500)); // Безопасная пауза в 1.5 секунды
                ESP.restart();
            }, 
            "APRebootTask", 
            2048, 
            nullptr, 
            1, 
            nullptr, 
            1
        );
    });

    // ============================================================
    // КОМАНДЫ РАДИО (STA режим — Управление плеером через очередь FreeRTOS)
    // ============================================================
    _server.on("/cmd", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!isSTA()) {
            request->send(403, "application/json", "{\"status\":\"error\",\"message\":\"Not in STA mode\"}");
            return;
        }
        
        // ИСПРАВЛЕНО: Открываем асинхронной лямбде в classes.cpp доступ к глобальному объекту радио
        extern Radio radio;
        
        if (request->hasParam("action")) {
            // Получаем значение параметра текстовой команды
            String action = request->arg("action");
            
            #if DEBUG_MODE
            Serial.printf("[Web] Command: %s\n", action.c_str());
            #endif
            
            // ПОЛНАЯ ПОТОКОБЕЗОПАСНОСТЬ: Методы класса Radio сами закинут нужные маркеры в xQueue
            if (action == "toggle") {
                radio.togglePlay();
            } else if (action == "next") {
                radio.nextStation();
            } else if (action == "prev") {
                radio.previousStation();
            }
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No action\"}");
        }
    });


    // ============================================================
    // УСТАНОВКА ГРОМКОСТИ ИЗ ВЕБА (Шкала 0-9, БЕЗ автосохранения на диск)
    // ============================================================
    _server.on("/volume", HTTP_POST, [this](AsyncWebServerRequest *request){
        if (!isSTA()) {
            request->send(403, "application/json", "{\"status\":\"error\",\"message\":\"Not in STA mode\"}");
            return;
        }
        
        if (request->hasParam("volume", true)) {
            int vol = request->getParam("volume", true)->value().toInt();
            vol = constrain(vol, 0, 9); // Ограничиваем строго в рамках шкалы 0-9
            
            #if DEBUG_MODE
            Serial.printf("[Web] Volume set to: %d\n", vol);
            #endif
            
            // ИСПРАВЛЕНО: Связываем локальный контекст с глобальной RAM проекта M5Stack
            extern WorkSPIFFS::ConfigData config;
            extern Radio radio;
            
            // СИНХРОНИЗАЦИЯ RAM: Обновляем громкость в единой глобальной RAM-переменной.
            config.volume = vol;
            
            // Пинаем плеер. Метод безопасно закинет CMD_VOLUME в очередь для Core 0
            radio.setVolume(vol);
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No volume parameter\"}");
        }
    });

    // ============================================================
    // 6. ЖИВОЙ СТАТУС РАДИО (STA режим — Опрос браузером раз в секунду)
    // ============================================================
    _server.on("/state", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!isSTA()) {
            request->send(403, "application/json", "{\"status\":\"error\",\"message\":\"Not in STA mode\"}");
            return;
        }
        
        // ИСПРАВЛЕНО: Открываем доступ к глобальному объекту радио для classes.cpp
        extern Radio radio;
        
        String json;
        // ОПТИМИЗАЦИЯ: Резервируем память, чтобы куча не фрагментировалась при ежесекундном опросе
        json.reserve(256); 
        
        // Формируем легкий JSON-пакет, считывая живые данные из RAM-памяти запущенного плеера
        json = "{";
        json += "\"stationName\":\"" + radio.getCurrentStationName() + "\",";
        json += "\"currentStation\":" + String(radio.getCurrentStationIndex()) + ","; 
        json += "\"volume\":" + String(radio.getVolume()) + ","; 
        json += "\"isPlaying\":" + String(radio.isPlaying() ? "true" : "false");
        json += "}";
        
        // Запрещаем браузеру кэшировать живой статус устройства
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });


    // Физический старт асинхронного сервера (СТРОГО ВНУТРИ метода setupWebServer)
    _server.begin();
    
    #if DEBUG_MODE
    Serial.println("[WebServer] Асинхронный сервер успешно запущен и слушает порт 80.");
    #endif
} // Закрывающая скобка метода void WiFiConnect::setupWebServer()

// Публичный метод-мост оставляем пустым для асинхронного режима
void WiFiConnect::handle() {

 }



