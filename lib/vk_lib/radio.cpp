// ============================================================
//  radio.cpp - Реализация класса Radio
// ============================================================

#include "radio.h"
#include "config.h"
#include "classes.h"
#include "display.h"
// #include "html_const.h"
#include <WiFi.h>
#include <M5Unified.h>

extern Display display;
extern WiFiConnect wifi;
extern WorkSPIFFS myFS;
extern WorkSPIFFS::ConfigData config;

QueueHandle_t globalRadioQueue = nullptr;
    // ============================================================
//  КОНСТРУКТОР
// ============================================================
Radio::Radio()
    : _isPlaying(false)
    , _lastNtpAttempt(0)       // ИСПРАВЛЕНО: инициализируем переменные автомата NTP
    , _ntpSuccess(false)       // ИСПРАВЛЕНО: стартовый флаг синхронизации времени
    , _lastNtpSync(0)
    , _screenInitialized(false)
    , _lastDisplayUpdate(0)
    , _config(nullptr)
    , _commandQueue(nullptr)
    , _currentMetadata("")
    , _scrollText("")
    , _scrollIndex(0)
    , _lastScrollMs(0)
{
    #if DEBUG_MODE
    Serial.println("[Radio] Constructor started");
    #endif

    // Создаем потокобезопасную очередь команд (Core 1 → Core 0)
    _commandQueue = xQueueCreate(10, sizeof(AudioMessage));
    globalRadioQueue = _commandQueue;
    if (_commandQueue == nullptr) {
        #if DEBUG_MODE
        Serial.println("[Radio] КРИТИЧЕСКАЯ ОШИБКА: Не удалось создать очередь команд!");
        #endif
    }

    #if DEBUG_MODE
    Serial.println("[Radio] Constructor completed");
    #endif
}

// ============================================================
//  ДЕСТРУКТОР
// ============================================================
Radio::~Radio() {
    #if DEBUG_MODE
    Serial.println("[Radio] Destructor called");
    #endif
    
    // 1. Физически останавливаем декодер и аудиопоток на Core 0
    stopPlaying();
    
    // 2. Безопасно очищаем и удаляем очередь FreeRTOS
    if (_commandQueue != nullptr) {
        AudioMessage dummy;
        // ИСПРАВЛЕНО: Выгребаем все оставшиеся сообщения, предотвращая утечку памяти в Heap
        while (xQueueReceive(_commandQueue, &dummy, 0) == pdTRUE) {
            // Просто освобождаем слоты памяти
        }
        // Удаляем саму очередь из оперативной памяти ESP32
        vQueueDelete(_commandQueue);
        _commandQueue = nullptr;
    }
}

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ И ЗАПУСК СИСТЕМЫ ПЛЕЕРА
// ============================================================
bool Radio::begin(WorkSPIFFS::ConfigData& config) {
#if DEBUG_MODE
    Serial.println("[Radio] Begin...");
    Serial.printf("[Radio] Station: %s\n", config.getCurrentStationName().c_str());
    Serial.printf("[Radio] Volume: %d\n", config.volume);
#endif

    // === ВЫВОД СТАТИЧЕСКОЙ ИНФОРМАЦИИ НА ЭКРАН ===
    display.drawStaticInfo(WiFi.localIP().toString());
    display.updateStationName(config.getCurrentStationName());
    display.currentVolume = -1;
    display.updateVolume(config.volume);
    display.drawTime(getTime());

    // === НАЧАЛЬНЫЙ СТАТУС ===
    display.updatePlayStatus(_isPlaying);

    // === ИНИЦИАЛИЗАЦИЯ GYRO JOYSTICK ===
    _joystick.begin(); // Аппаратный запуск и автокалибровка нуля в покое

    // ============================================================
    //  АНТИ-ФРАГМЕНТАЦИЯ: Бронируем буфер MP3-декодера в чистой куче!
    // ============================================================
    // Настраиваем ножки I2S звукового чипа Hat SPK2 ПРЯМО СЕЙЧАС, пока память чиста
    _audio.setPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN);
    
    // ВАРИАНТ А: Вызываем встроенный метод библиотеки, который принудительно 
    // выделяет память под MP3-декодер прямо сейчас, не дожидаясь подключения к сети!
    // (Если в вашей версии библиотеки этот метод называется иначе, используем Вариант Б)
//    _audio.allocate_buffers(); 

    // ВАРИАНТ Б (Если Вариант А не скомпилируется): 
    // Скармливаем синтаксически корректный, но несуществующий локальный хост. 
    // Библиотека принудительно инициализирует и зафиксирует в RAM буферы MP3-декодера, 
    // затем тихо вылетит по тайм-ауту сети, но ПАМЯТЬ ОСТАНЕТСЯ ЗАБРОНИРОВАНА ЗА НАМИ!
    // _audio.connecttohost("http://localhost/fake.mp3"); 

    // ============================================================
    //  СОЗДАНИЕ ЗАДАЧ FREERTOS
    // ============================================================

    // --- Задача 1: Аудиопоток (Строго на CORE 0) ---
    xTaskCreatePinnedToCore(
        [](void* param) {
            Radio* radio = (Radio*)param;
            vTaskDelay(2000 / portTICK_PERIOD_MS); // Даем системе 2 сек полностью поднять сетевые сокеты
            #if DEBUG_MODE
            Serial.println("[AudioTask] Directly starting stream on Core 0...");
            #endif
            while (true) {
                radio->runAudio(); // Крутит буферы и декодер I2S
                vTaskDelay(AUDIO_TASK_DELAY_MS / portTICK_PERIOD_MS);
            }
        }, 
        "AudioTask", 
        AUDIO_TASK_STACK_SIZE, 
        this, 
        AUDIO_TASK_PRIORITY, 
        nullptr, 
        0 // Строго Core 0
    );

    // --- Задача 2: Управление кнопками и логикой (CORE 1) ---
    xTaskCreatePinnedToCore(
        [](void* param) {
            Radio* radio = (Radio*)param;
            while (true) {
                radio->runControl();
                vTaskDelay(CONTROL_TASK_DELAY_MS / portTICK_PERIOD_MS);
            }
        }, 
        "ControlTask", 
        CONTROL_TASK_STACK_SIZE, 
        this, 
        CONTROL_TASK_PRIORITY, 
        nullptr, 
        1 // Core 1
    );

    // --- Задача 3: Сетевые события веб-сервера и NTP (CORE 1) ---
    xTaskCreatePinnedToCore(
        [](void* param) {
            Radio* radio = (Radio*)param;
            radio->updateTime(); // Первая мягкая попытка вывода времени
            while (true) {
                radio->runNetwork();
                vTaskDelay(NETWORK_TASK_DELAY_MS / portTICK_PERIOD_MS);
            }
        }, 
        "NetworkTask", 
        NETWORK_TASK_STACK_SIZE, 
        this, 
        NETWORK_TASK_PRIORITY, 
        nullptr, 
        1 // Core 1
    );

    // --- Задача 4: Опрос гироскопа (Строго на CORE 1 с точным таймером) ---
    xTaskCreatePinnedToCore(
        [](void* param) {
            Radio* radio = (Radio*)param;
            TickType_t xLastWakeTime = xTaskGetTickCount();
            const TickType_t xFrequency = pdMS_TO_TICKS(GYRO_SAMPLE_INTERVAL_MS);
            #if DEBUG_MODE
            Serial.println("[GyroTask] Высокоточный опрос гироскопа запущен на Core 1");
            #endif
            while (true) {
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
                radio->runGyroJoystick(); // Вызываем шаг обработки джойстика
            }
        }, 
        "GyroTask", 
        GYRO_TASK_STACK_SIZE, 
        this, 
        GYRO_TASK_PRIORITY, 
        nullptr, 
        1 // Core 1
    );

#if DEBUG_MODE
    Serial.println("[Radio] Begin completed");
#endif
    return true;
}

// ============================================================
//  УПРАВЛЕНИЕ ВОСПРОИЗВЕДЕНИЕМ (Вызывается на Core 1 → Отправляет на Core 0)
// ============================================================

void Radio::play() {
    if (_commandQueue == nullptr) {
        #if DEBUG_MODE
        Serial.println("[Radio Core1] Failed to send play command: No queue!");
        #endif
        return;
    }
    AudioMessage msg = {CMD_PLAY, 0};
    xQueueSend(_commandQueue, &msg, 0); // Отправляем мгновенно без блокировки задачи
}

void Radio::stop() {
    if (_commandQueue == nullptr) return;
    AudioMessage msg = {CMD_STOP, 0};
    xQueueSend(_commandQueue, &msg, 0);
}

void Radio::togglePlay() {
    if (_commandQueue == nullptr) return;
    
    // ИСПРАВЛЕНО: Вместо прямого вызова _audio, просто отправляем флаг переключения в очередь.
    // runGyroJoystick() и кнопки вызовут именно этот безопасный метод!
    AudioMessage msg = {CMD_TOGGLE, 0};
    xQueueSend(_commandQueue, &msg, 0);
    
    #if DEBUG_MODE
    Serial.println("[Radio Core1] Команда CMD_TOGGLE отправлена в очередь");
    #endif
}

void Radio::forceConnect() {
    if (_commandQueue == nullptr) return;

    // Принудительное подключение при старте — это отправка команды CMD_PLAY
    AudioMessage msg = {CMD_PLAY, 0};
    xQueueSend(_commandQueue, &msg, 0);
    
    #if DEBUG_MODE
    Serial.println("[Radio Core1] Команда forceConnect (CMD_PLAY) отправлена в очередь");
    #endif
}

// ============================================================
//  ВНУТРЕННИЕ ПРИВАТНЫЕ МЕТОДЫ ДЛЯ СТРИМА (Выполняются СТРОГО на Core 0)
// ============================================================


void Radio::stopPlaying() {
    #if DEBUG_MODE
    Serial.println("[AudioCore0] Физическая остановка декодера I2S");
    #endif

    _audio.stopSong();              // Останавливаем физический декодер
    _isPlaying = false;
    display.updatePlayStatus(false); // Выводим красный статус STOP на экран
    _currentMetadata = "";
    display.updateMetadata("");     // Очищаем экран от старого названия трека
}

// ============================================================
//  УПРАВЛЕНИЕ СТАНЦИЯМИ И ГРОМКОСТЬЮ (Вызываются на Core 1 -> Отправляют на Core 0)
// ============================================================

// ============================================================
//  МЕТОДЫ ОТПРАВКИ КОМАНД В ОЧЕРЕДЬ (Выполняются на Core 1)
// ============================================================

void Radio::nextStation() {
    if (_commandQueue == nullptr) return;

    // БЕЗОПАСНО ДЛЯ ЛЕНТЯЕВ: Никаких блокировок по флагу _isPlaying!
    // Просто закидываем "записку" в почтовый ящик, а вся магия авто-стопа 
    // и авто-старта выполнится упорядоченно на Core 0 внутри processCommand.
    AudioMessage msg = {CMD_NEXT, 0};
    xQueueSend(_commandQueue, &msg, 0);

    #if DEBUG_MODE
    Serial.println("[Radio Core1] Безусловная команда CMD_NEXT отправлена в очередь");
    #endif
}

void Radio::previousStation() {
    if (_commandQueue == nullptr) {
        #if DEBUG_MODE
        Serial.println("[ERROR Core1] Command queue is null in previousStation!");
        #endif
        return;
    }

    // БЕЗОПАСНО ДЛЯ ЛЕНТЯЕВ: Шлем команду переключения назад без задержек и фильтров
    AudioMessage msg = {CMD_PREV, 0};
    xQueueSend(_commandQueue, &msg, 0);

    #if DEBUG_MODE
    Serial.println("[Radio Core1] Безусловная команда CMD_PREV отправлена в очередь");
    #endif
}


void Radio::setVolume(int vol) {
    if (_commandQueue == nullptr) {
        #if DEBUG_MODE
        Serial.println("[ERROR Core1] Command queue is null in setVolume!");
        #endif
        return;
    }
    
    // ИСПРАВЛЕНО: Ограничиваем громкость строго в рамках шкалы 0-9
    vol = constrain(vol, 0, 9); 
    
    AudioMessage msg = {CMD_VOLUME, vol};
    xQueueSend(_commandQueue, &msg, 0);
}

// ============================================================
//  ОПТИМИЗИРОВАННЫЙ ЦИКЛ ДЕКОДЕРА АУДИО (Строго на Core 0)
// ============================================================
void Radio::runAudio() {
    if (_commandQueue == nullptr) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return;
    }

    // 1. Атомарная проверка команд из очереди FreeRTOS (0 тиков ожидания, чтобы не тормозить цикл)
    AudioMessage msg;
    if (xQueueReceive(_commandQueue, &msg, 0) == pdTRUE) {
        processCommand(msg); // Физически переключаем станции/громкость на Core 0
    }

    // 2. Декодирование и прокрутка аудиопотока
    if (_isPlaying) {
        // Наполняем I2S-буфер на максимальной скорости процессора
        _audio.loop();
        
        #if DEBUG_MODE
        static unsigned long lastAudioLog = 0;
        if (millis() - lastAudioLog > 5000) { // Лог раз в 5 секунд
            lastAudioLog = millis();
            Serial.printf("[AudioTask0] Стрим активен. Свободно Heap: %d байт\n", ESP.getFreeHeap());
        }
        #endif
    } else {
        // Если радио выключено или на паузе — честно усыпляем задачу на 10 мс, разгружая ядро
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// ============================================================
//  ЗАДАЧА СЕТЕВОГО МОНИТОРИНГА И NTP ВРЕМЕНИ (Строго на Core 1)
// ============================================================
void Radio::runNetwork() {
    // =========================================================================
    // ШАГ 1: ОБЯЗАТЕЛЬНОЕ ОБСЛУЖИВАНИЕ СИНХРОННОГО ВЕБ-СЕРВЕРА КЛАССА
    // =========================================================================
    // Помещено на самый верх! Сервер должен отвечать на запросы /state, /volume
    // и /cmd как при успешном Wi-Fi (STA), так и в аварийном режиме AP!
    extern WiFiConnect wifi;
    wifi.handle();
    // =========================================================================

    // Вся сетевая активность крутится на Core 1 каждые 100 мс (NETWORK_TASK_DELAY_MS)
    unsigned long now = millis();

    // 1. КОНТРОЛЬ И МОНИТОРИНГ WI-FI СОЕДИНЕНИЯ
    if (WiFi.status() != WL_CONNECTED) {
        #if DEBUG_MODE
        static unsigned long lastWifiCheck = 0;
        if (now - lastWifiCheck > 5000) { // Сообщаем о проблеме раз в 5 секунд
            lastWifiCheck = now;
            Serial.println("[NetworkTask1] Внимание: Wi-Fi соединение разорвано! Ожидание...");
        }
        #endif

        // Если мы играли, но сеть упала — временно глушим поток, чтобы буфер не завис
        if (_isPlaying) {
            _audio.stopSong();
            display.updatePlayStatus(false);
        }
        return; // Выходим из цикла обработки, пока сеть не восстановится
    }

    // 2. АСИНХРОННЫЙ АВТОМАТ СИНХРОНИЗАЦИИ NTP ВРЕМЕНИ
    // Проверяем, пора ли обновить часы (раз в час при успехе или раз в 20 сек при ошибке)
    unsigned long ntpInterval = _ntpSuccess ? (NTP_SUCCESS_INTERVAL * 1000) : (NTP_RETRY_INTERVAL * 1000);
    
    if (_lastNtpAttempt == 0 || (now - _lastNtpAttempt > ntpInterval)) {
        _lastNtpAttempt = now;
        
        #if DEBUG_MODE
        Serial.println("[NetworkTask1] Запуск фоновой синхронизации времени с NTP...");
        #endif

        // Инициализируем системный NTP-клиент ESP32 (неблокирующая отправка UDP пакета)
        int timezoneOffset = (int)(NTP_TIMEZONE_OFFSET * 3600);
        configTime(timezoneOffset, 0, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
        
        // Мягко проверяем, обновились ли системные часы
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 15)) { // Ждем ответ всего 15 мс, чтобы не вешать веб-сервер
            _timeInfo = timeinfo;
            _ntpSuccess = true;
            _lastNtpSync = now;
            
            // Отправляем точное время на экран M5Stick
            display.drawTime(getTime()); 
            
            #if DEBUG_MODE
            Serial.printf("[NetworkTask1] NTP Успех! Текущее время: %s\n", getTime().c_str());
            #endif
        } else {
            _ntpSuccess = false;
            #if DEBUG_MODE
            Serial.println("[NetworkTask1] NTP Ошибка: Сервер не ответил. Повтор через 20 секунд...");
            #endif
        }
    }

    // 3. ПЕРИОДИЧЕСКОЕ ОБНОВЛЕНИЕ МИНУТ НА ЭКРАНЕ (Каждые 30 секунд)
    static unsigned long lastClockUpdate = 0;
    if (_ntpSuccess && (now - lastClockUpdate > 30000)) {
        lastClockUpdate = now;
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 0)) {
            _timeInfo = timeinfo;
            display.drawTime(getTime()); // Мягко обновляем часы на экране
        }
    }
}
// ============================================================
//  ОБРАБОТКА ФИЗИЧЕСКИХ КНОПОК (Выполняется на Core 1)
// ============================================================
void Radio::handleButtons() {
    unsigned long now = millis();
    
    // Читаем физические пины нижней гребенки Core2 по вашим родным макросам
    bool pressLeft   = (digitalRead(BTN_A_PIN) == LOW);
    bool pressMiddle = (digitalRead(BTN_B_PIN) == LOW);
    bool pressRight  = (digitalRead(BTN_C_PIN) == LOW);

    // 1. ЛЕВАЯ КНОПКА (BTN_A) — Громкость -1 по кругу
    static bool leftTrigger = false;
    if (pressLeft && !leftTrigger) {
        leftTrigger = true;
        int v = config.volume - 1;
        if (v < 0) v = 9;
        if (_commandQueue != nullptr) { AudioMessage msg = {CMD_VOLUME, v}; xQueueSend(_commandQueue, &msg, 0); }
    } else if (!pressLeft) leftTrigger = false;

    // 2. ЦЕНТРАЛЬНАЯ КНОПКА (BTN_B) — PLAY / STOP, долгое нажатие (>1 сек) — СОХРАНЕНИЕ
    static unsigned long midStartMs = 0;
    static bool midLongTriggered = false;
    if (pressMiddle) {
        if (midStartMs == 0) { midStartMs = now; midLongTriggered = false; }
        if (!midLongTriggered && (now - midStartMs >= 1000)) {
            midLongTriggered = true;
            if (_commandQueue != nullptr) { AudioMessage msg = {CMD_SAVE, 0}; xQueueSend(_commandQueue, &msg, 0); }
        }
    } else {
        if (midStartMs > 0) {
            if (!midLongTriggered && (now - midStartMs < 1000)) {
                if (_commandQueue != nullptr) { AudioMessage msg = {CMD_TOGGLE, 0}; xQueueSend(_commandQueue, &msg, 0); }
            }
            midStartMs = 0;
        }
    }

    // 3. ПРАВАЯ КНОПКА (BTN_C) — Следующая станция ВПЕРЕД по кругу
    static bool rightTrigger = false;
    if (pressRight && !rightTrigger) {
        rightTrigger = true;
        if (_commandQueue != nullptr) { AudioMessage msg = {CMD_NEXT, 0}; xQueueSend(_commandQueue, &msg, 0); }
    } else if (!pressRight) rightTrigger = false;
}


// ============================================================
//  startPlaying() - ФИЗИЧЕСКИЙ ЗАПУСК ИНТЕРНЕТ-СТРИМА (Core 0)
// ============================================================
void Radio::startPlaying() {
    if (config.stationCount == 0) return;

    #if DEBUG_MODE
    Serial.printf("[Core2] Подключение к Bluetooth колонке и хосту: %s\n", config.getCurrentStationName().c_str());
    #endif

    // === ПЕРЕКЛЮЧАЕМ ДВИЖОК НА BLUETOOTH ===
    // Вместо физических ножек setPinout мы говорим библиотеке запустить 
    // встроенный Bluetooth-передатчик (A2DP Source) и привязаться к имени вашей колонки!
    // Замените "ИМЯ_ВАШЕЙ_КОЛОНКИ" на точное имя (например, "JBL Flip 6" или "Sony SRS-XB13")
//    _audio.connectToBluetooth("JBL"); 
    
    // Выставляем системную громкость (библиотека сама смасштабирует её для Bluetooth протокола)
    _audio.setVolume(config.volume * 2 + 3);

    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Открываем потоковое вещание по URL из config
    _audio.connecttohost(config.getCurrentStationURL().c_str());
    _isPlaying = true;
}


// ============================================================
//  ПОТОКОБЕЗОПАСНЫЕ ГЕТТЕРЫ ИНТЕРФЕЙСА
// ============================================================
int Radio::getVolume() const {
    // Возвращаем живую громкость напрямую из глобального RAM-контейнера config
    return config.volume; 
}

int Radio::getCurrentStationIndex() const {
    // Возвращаем живой индекс станции напрямую из глобального config
    return config.currentStation; 
}

// ============================================================
//  ГЕТТЕРЫ КЛАССА RADIO ДЛЯ ВЕБ-ИНТЕРФЕЙСА И ЭКРАНА
// ============================================================

String Radio::getCurrentStationName() const {
    // ИСПРАВЛЕНО: Читаем имя текущей станции напрямую из глобального config в RAM
    return config.getCurrentStationName();
}

String Radio::getCurrentStationURL() const {
    // ИСПРАВЛЕНО: Читаем URL потока напрямую из глобального config
    return config.getCurrentStationURL();
}

String Radio::getIP() const {
    return WiFi.localIP().toString();
}

int Radio::getStationCount() const {
    return _config ? _config->stationCount : 0;
}

String Radio::getStationName(int index) const {
    if (_config && index >= 0 && index < _config->stationCount) {
        return _config->stations[index].name;
    }
    return "";
}

String Radio::getStationURL(int index) const {
    if (_config && index >= 0 && index < _config->stationCount) {
        return _config->stations[index].url;
    }
    return "";
}

// ============================================================
//  АСИНХРОННАЯ СИНХРОНИЗАЦИЯ СИСТЕМНОГО ВРЕМЕНИ (NTP на Core 1)
// ============================================================

void Radio::updateTime() {
    time_t now = time(nullptr); // Считываем текущее внутреннее время контроллера
    
    // Выбираем интервал опроса в зависимости от того, была ли прошлая синхронизация успешной
    time_t interval = _ntpSuccess ? NTP_SUCCESS_INTERVAL : NTP_RETRY_INTERVAL;
    
    // Если время для фонового обновления подошло — запускаем мягкую синхронизацию
    if (now - _lastNtpAttempt >= interval) {
        syncNTP();
    }
}

void Radio::syncNTP() {
    _lastNtpAttempt = time(nullptr);  // Запоминаем время текущей попытки
    
    // Переводим смещение часового пояса из часов в секунды (UTC+3 = 10800 секунд)
    int timezoneOffset = (int)(NTP_TIMEZONE_OFFSET * 3600);
    
    // Мягко инициализируем встроенный NTP-клиент ESP32 (неблокирующая отправка пакета)
    configTime(timezoneOffset, 0, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
    
    struct tm timeinfo;
    // КРИТИЧЕСКИЙ ФИКС: Ожидаем ответ от системных регистров времени всего 15 миллисекунд!
    // Это полностью исключает пятисекундное зависание веб-сервера и сети при сбоях связи.
    if (getLocalTime(&timeinfo, 15)) {  
        _timeInfo = timeinfo;
        _ntpSuccess = true;  // Запускаем флаг успеха
        
        #if DEBUG_MODE
        Serial.printf("[NTP Успех] Точное время установлено: %s\n", getTime().c_str());
        #endif
        
        // Отправляем новое точное время на графический дисплей M5Stick
        display.drawTime(getTime()); 
    } else {
        _ntpSuccess = false; // Сервер еще не прислал UDP-ответ, система повторит попытку асинхронно
        
        #if DEBUG_MODE
        Serial.printf("[NTP Ожидание] Время пока не получено. Повторный опрос через %d сек...\n", NTP_RETRY_INTERVAL);
        #endif
    }
}
// ============================================================
//  СТАРЫЙ МЕРТВЫЙ КОД АКСЕЛЕРОМЕТРА ПОЛНОСТЬЮ УДАЛЕН ОТСЮДА
// ============================================================
// Методы handleTilt() и readAccelerometer() стерты. 
// Вся обработка идет в runGyroJoystick() через класс GyroJoystick.


// ============================================================
//  ИНИЦИАЛИЗАЦИЯ ИНТЕРФЕЙСА ЭКРАНА (Выполняется на Core 1)
// ============================================================
void Radio::initScreen() {
    if (_screenInitialized) return;
    
    // КРИТИЧЕСКИЙ ФИКС: Удален повторный вызов M5.begin()!
    // Железо M5StickC Plus безопасно инициализируется один раз в setup() главного файла.

    M5.Lcd.setRotation(3);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    
    // ИСПРАВЛЕНО: Управляем подсветкой экрана через макрос по умолчанию из вашего config.h
    M5.Display.setBrightness(DEFAULT_BRIGHTNESS);
    
    _screenInitialized = true;
    _lastDisplayUpdate = 0;
    
    #if DEBUG_MODE
    Serial.println("[Screen Core1] Графический интерфейс успешно настроен");
    #endif
}

// ============================================================
//  showStartScreen() - Асинхронный стартовый экран (Core 1)
// ============================================================
void Radio::showStartScreen() {
    if (!_screenInitialized) initScreen();
    
    // ИСПРАВЛЕНО: Делегируем отрисовку специализированному объекту дисплея,
    // передавая ему актуальные данные для витрины. Это убирает визуальное мерцание.
    display.showStartup("VK-Radio v2.0", _config ? _config->stationCount : 0);
    
    // КРИТИЧЕСКИЙ ФИКС: УДАЛЕН блокирующий delay(2000)!
    // Заставка нарисовалась мгновенно, и задача управления сразу продолжает 
    // работу, не замораживая веб-сервер и опрос гироскопа.
    #if DEBUG_MODE
    Serial.println("[Screen Core1] Отображен стартовый экран");
    #endif
}
// ============================================================
//  showError() - Асинхронное сообщение об ошибке (Core 1)
// ============================================================
void Radio::showError(const String& message) {
    if (!_screenInitialized) initScreen();
    
    // ИСПРАВЛЕНО: Передаем отрисовку специализированному объекту дисплея
    display.showError(message);
    
    // КРИТИЧЕСКИЙ ФИКС: УДАЛЕН блокирующий delay(2000)!
    // Управление мгновенно возвращается планировщику FreeRTOS,
    // веб-сервер и гироскоп продолжают работать в штатном режиме.
    #if DEBUG_MODE
    Serial.printf("[Screen Core1] Зафиксирована ошибка: %s\n", message.c_str());
    #endif
}

// ============================================================
//  ОТРИСОВКА ИНТЕРФЕЙСА ДИСПЛЕЯ (Выполняется на Core 1)
// ============================================================

void Radio::drawStatus() {
    M5.Lcd.setTextSize(2);
    // ИСПРАВЛЕНО: Добавлен черный фон под текстом, чтобы буквы затирали сами себя
    M5.Lcd.setTextColor(_isPlaying ? TFT_GREEN : TFT_RED, TFT_BLACK);
    M5.Lcd.setCursor(5, 5);
    
    // Синхронизируем выводимый текст с реальным флагом воспроизведения
    _displayStatus = _isPlaying ? "PLAYING" : "STOPPED";
    
    // Дописываем пробел, чтобы слово "STOPPED" чисто затиралось словом "PLAYING"
    String statusStr = _displayStatus;
    if (_isPlaying) statusStr += " "; 
    
    M5.Lcd.print(statusStr);
}

void Radio::drawStationName() {
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK); // ИСПРАВЛЕНО: Защита от наложения букв
    M5.Lcd.setCursor(5, 35);
    
    String name = _displayStation;
    if (name.length() > 18) {
        name = name.substring(0, 16) + "..";
    }
    
    // ИСПРАВЛЕНО: Дописываем пробелы в конец, чтобы полностью стереть остатки прошлого длинного имени
    while (name.length() < 18) {
        name += " ";
    }
    M5.Lcd.print(name);
}

void Radio::drawMetadata() {
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK); // ИСПРАВЛЕНО: Защита от визуального шума
    M5.Lcd.setCursor(5, 65);
    
    String metadata = _displayMetadata;
    if (metadata.length() > 30) {
        metadata = metadata.substring(0, 28) + "..";
    }
    
    // Затираем пробелами хвост прошлой песни
    while (metadata.length() < 30) {
        metadata += " ";
    }
    M5.Lcd.print(metadata);
}

void Radio::drawTime() {
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK); // ИСПРАВЛЕНО: убрано мерцание часов
    M5.Lcd.setCursor(180, 5);
    M5.Lcd.print(_displayTime);
}

// ============================================================
//  drawVolumeBar() - Оптимизированная полоска громкости (шкала 0-9)
// ============================================================
void Radio::drawVolumeBar() {
    int barX = 210;
    int barY = 5;
    int barWidth = 20;
    int barHeight = 80;
    
    // Формула сохранена для вашей шкалы 0-9
    int fillHeight = (config.volume * barHeight) / 10;
    
    // ИСПРАВЛЕНО: Статический буфер предотвращает перерисовку прямоугольников,
    // если громкость не менялась. Экран больше не пульсирует!
    static int lastDrawnVolume = -1;
    if (config.volume != lastDrawnVolume) {
        lastDrawnVolume = config.volume;

        // Полностью очищаем фон полоски темным цветом
        M5.Lcd.fillRect(barX, barY, barWidth, barHeight, TFT_DARKGREY);
        
        // Отрисовка уровня заполнения (цветовые пороги адаптированы под шкалу 0-9)
        if (fillHeight > 0) {
            uint16_t color;
            if (config.volume <= 2) {         // До 2 делений — тихий красный
                color = TFT_RED;
            } else if (config.volume <= 6) {  // До 6 делений — комфортный желтый
                color = TFT_YELLOW;
            } else {                           // Выше 6 делений — громкий зеленый
                color = TFT_GREEN;
            }
            // Рисуем заполнение шкалы снизу вверх
            M5.Lcd.fillRect(barX, barY + barHeight - fillHeight, barWidth, fillHeight, color);
        }
        
        // Рисуем аккуратную внешнюю белую рамку вокруг шкалы
        M5.Lcd.drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);
    }
    
    // Вывод цифрового значения под полоской
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(barX + 6, barY + barHeight + 5);
    M5.Lcd.print(String(config.volume));
}
// ============================================================
//  drawChannelNumber() - Номер канала (Выполняется на Core 1)
// ============================================================
void Radio::drawChannelNumber() {
    M5.Lcd.setTextSize(1);
    // ИСПРАВЛЕНО: Добавлен черный фон под текстом, чтобы цифры затирали сами себя
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK); 
    M5.Lcd.setCursor(180, 25);
    
    // Форматируем строку пробелом, чтобы каналы "9" и "10" корректно стирали друг друга
    String chStr = "CH " + String(config.currentStation + 1);
    if (config.currentStation < 9) {
        chStr += " "; 
    }
    M5.Lcd.print(chStr);
}

// ============================================================
//  drawProgressBar() - Универсальный прогресс-бар (Core 1)
// ============================================================
void Radio::drawProgressBar(int x, int y, int width, int height, 
                            int value, int maxValue, uint16_t color) {
    // ИСПРАВЛЕНО: Защита от деления на ноль, если maxValue прилетит пустым
    if (maxValue <= 0) maxValue = 1;
    
    int fillWidth = (value * width) / maxValue;
    if (fillWidth > width) fillWidth = width; // Страховка от вылета за границы рамки
    
    M5.Lcd.fillRect(x, y, width, height, TFT_DARKGREY);
    if (fillWidth > 0) {
        M5.Lcd.fillRect(x, y, fillWidth, height, color);
    }
    M5.Lcd.drawRect(x, y, width, height, TFT_WHITE);
}

// ============================================================
//  clearScreen() - Очистка экрана
// ============================================================
void Radio::clearScreen() {
    if (_screenInitialized) {
        M5.Lcd.fillScreen(BLACK);
    }
}

// ============================================================
//  getTime() - Получить текущее время в формате "HH:MM:SS" (Core 1)
// ============================================================
String Radio::getTime() const {
    // Если NTP-сервер еще ни разу успешно не ответил, возвращаем заглушку
    if (!_ntpSuccess) {
        return "--:--:--";  
    }
    
    time_t now = time(nullptr);  // Системное время ESP32
    struct tm timeinfo;
    localtime_r(&now, &timeinfo); // Потокобезопасное чтение структуры времени
    
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
    return String(buffer);
}

// ============================================================
//  СИСТЕМНЫЕ CALLBACK'И ДЛЯ МЕТАДАННЫХ (Вызываются СТРОГО на Core 0)
// ============================================================

// Так как колбэки библиотеки — это обычные Си-функции вне класса, 
// им нужен доступ к нашему объекту Radio для передачи пойманного названия песни.
extern Radio radio; 

void audio_showstation(const char* info) {
    #if DEBUG_MODE
    Serial.printf("[AudioCore0 Callback] Station: %s\n", info);
    #endif
}

void audio_info(const char* info) {
    #if DEBUG_MODE
    Serial.printf("[AudioCore0 Callback] Info: %s\n", info);
    #endif
}

// ============================================================
//  ФИЗИЧЕСКОЕ ВЫПОЛНЕНИЕ КОМАНД (Выполняется СТРОГО на Core 0)
// ============================================================
void Radio::processCommand(const AudioMessage& msg) { 
    switch (msg.command) { 
        
        case CMD_SAVE:
            #if DEBUG_MODE
            Serial.println("[AudioCore0] Извлечена команда CMD_SAVE. Принудительный STOP и запись на диск...");
            #endif

            // Безопасно выключаем музыку, освобождая буферы и процессор перед прожигом Flash
            if (_isPlaying) {
                stopPlaying();
            }

            // Переводим экран в режим STOP и пишем красивую рамку плеера
            display.drawStaticInfo(wifi.getIPAddress());
            display.updateStationName(config.getCurrentStationName());
            display.updateVolume(config.volume);
            display.drawTime(getTime());
            display.updatePlayStatus(false); // Нарисует красный статус STOP

            // Физически записываем живой конфиг из оперативной памяти RAM на Flash-память
            if (myFS.saveConfig(config)) {
                display.updateMetadata("✅ Saved successfully in STOP!");
            } else {
                display.updateMetadata("❌ Flash write error!");
            }
            break;

        case CMD_VOLUME:
            // Принимаем точное, вычисленное кнопкой или джойстиком значение (шкала 0-9)
            config.volume = msg.value;
            // Строгая защита от вылета за границы рабочей шкалы
            if (config.volume > 9) config.volume = 9;
            if (config.volume < 0) config.volume = 0;
            
            // Применяем формулу для железного уровня громкости библиотеки Audio.h
            _audio.setVolume(config.volume * 2 + 3);
            
            // Обновляем графическую шкалу на экране M5Stick
            display.updateVolume(config.volume);
            
            #if DEBUG_MODE
            Serial.printf("[AudioCore0] Установлена громкость: %d/9\n", config.volume);
            #endif
            break;

        case CMD_TOGGLE:
            // ЧИСТЫЙ ОТКАТ: Никаких проверок на маркер 99! Только исходный Play/Stop
            _isPlaying = !_isPlaying;
            if (!_isPlaying) {
                stopPlaying();
                display.drawStaticInfo(wifi.getIPAddress());
                display.updateStationName(config.getCurrentStationName());
                display.updateVolume(config.volume);
                display.drawTime(getTime());
                display.updatePlayStatus(false);
            } else {
                startPlaying();
            }
            break;

        case CMD_NEXT: {
            // Запоминаем стартовый статус: играла ли музыка в момент нажатия кнопки/жеста
            bool wasPlayingBefore = _isPlaying;
            
            // 1. АВТО-СТОП: Если стрим активен, мягко глушим текущую станцию на Core 0
            if (wasPlayingBefore) {
                stopPlaying();
            }
            
            // 2. Линейно шагаем по индексу в глобальной оперативной памяти config
            config.currentStation++;
            if (config.currentStation >= config.stationCount) {
                config.currentStation = 0; // Закольцевали с 9-й на 0-ю
            }
            
            // 3. Мгновенно выводим новое текстовое имя и номер канала на экран
            display.updateStationName(config.getCurrentStationName());
            drawChannelNumber();
            
            #if DEBUG_MODE
            Serial.printf("[AudioCore0] Канал переключен ВПЕРЕД. Индекс: %d, Название: %s\n", config.currentStation, config.getCurrentStationName().c_str());
            #endif
            
            // 4. АВТО-СТАРТ: Если музыка играла до переключения — мгновенно запускаем новый поток!
            if (wasPlayingBefore) {
                startPlaying(); // Сам откроет новый URL из config и зажжет зеленый PLAY
            }
            break;
        }

        case CMD_PREV: {
            bool wasPlayingBefore = _isPlaying;
            
            // 1. АВТО-СТОП
            if (wasPlayingBefore) {
                stopPlaying();
            }
            
            // 2. Круговое переключение назад прямо в RAM
            config.currentStation--;
            if (config.currentStation < 0) {
                config.currentStation = config.stationCount - 1; // С 0-й прыгаем на 9-ю
            }
            
            // 3. Обновляем графику на дисплее
            display.updateStationName(config.getCurrentStationName());
            drawChannelNumber();
            
            #if DEBUG_MODE
            Serial.printf("[AudioCore0] Канал переключен НАЗАД. Индекс: %d, Название: %s\n", config.currentStation, config.getCurrentStationName().c_str());
            #endif
            
            // 4. АВТО-СТАРТ
            if (wasPlayingBefore) {
                startPlaying();
            }
            break;
        }
    } // Конец конструкции switch (msg.command)
} // Финальная закрывающая скобка всего метода processCommand

// ============================================================
//  ОБРАБОТКА КОМАНД С ГИРОСКОПА (Выполняется на Core 1 каждые 20 мс)
// ============================================================
void Radio::runGyroJoystick() {
    // Получаем импульсную команду от асинхронного алгоритма накопления жестов (0-5)
    int cmd = _joystick.update();
    
    if (cmd != GYRO_CMD_NONE) {
        #if DEBUG_MODE
        Serial.printf("[Gyro Core1] Жест распознан! Код команды: %d\n", cmd);
        #endif
        
        switch (cmd) {
            case GYRO_CMD_SHAKE:
                togglePlay();           // Безопасно отправляет CMD_TOGGLE в очередь
                break;
            case GYRO_CMD_ROLL_UP:
                setVolume(config.volume + 1); // Безопасно отправляет CMD_VOLUME с новым шагом вверх
                break;
            case GYRO_CMD_ROLL_DOWN:
                setVolume(config.volume - 1); // Безопасно отправляет CMD_VOLUME с новым шагом вниз
                break;
            case GYRO_CMD_PITCH_FWD:
                nextStation();          // Безопасно отправляет CMD_NEXT в очередь
                break;
            case GYRO_CMD_PITCH_BWD:
                previousStation();      // Безопасно отправляет CMD_PREV в очередь
                break;
            default:
                break;
        }
    }
}

// ============================================================
//  ЗАДАЧА УПРАВЛЕНИЯ КНОПКАМИ И ИНТЕРФЕЙСОМ (Выполняется на Core 1 каждые 50 мс)
// ============================================================
void Radio::runControl() {
    if (_commandQueue == nullptr) {
        #if DEBUG_MODE
        Serial.println("[ERROR Core1] Очередь команд пуста в runControl!");
        #endif
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return;
    }
    
    // Опрашиваем физические кнопки M5 и боковую (антидребезг внутри)
    handleButtons();
    
    // === ОБНОВЛЕНИЕ ЧАСОВ НА ЭКРАНЕ КАЖДУЮ СЕКУНДУ ===
    static unsigned long lastSecond = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastSecond >= 1000) {
        lastSecond = currentTime;
        // Мягко обновляем секунды времени "HH:MM:SS" на экране из буфера времени NTP
        display.drawTime(getTime());
    }
    
    // 2. ИСПРАВЛЕНО: Каждые 50 мс проверяем, пора ли сдвинуть буквы бегущей строки трека.
    // Метод handleMarquee сам отсчитает свои 350 мс без блокировок FreeRTOS!
    handleMarquee();
}

// ============================================================
//  КОЛБЭК: АВТОМАТИЧЕСКИЙ ПЕРЕХВАТ МЕТАДАННЫХ ТРЕКА (Core 0)
// ============================================================
void audio_showstreamtitle(const char *info) {
    if (info == nullptr) return;
    
    // Даем таске на Core 1 доступ к методам плеера
    extern Radio radio;
    
    String title = String(info);
    title.trim(); // Убираем мусорные пробелы по бокам
    
    if (title.length() > 0) {
        #if DEBUG_MODE
        Serial.printf("[AudioCore0 Callback] Поймана песня: %s\n", title.c_str());
        #endif
        
        // Передаем текст во внутренний обработчик класса Radio
        // Мы добавляем 10 пробелов в конец, чтобы при прокрутке 
        // конец песни не слипался с её началом!
        radio.setScrollText(title + "          "); 
    }
}

// Метод вызывается из колбэка на Core 0, чтобы обновить текст песни
void Radio::setScrollText(const String& text) {
    _scrollText = text;
    _scrollIndex = 0;
    _lastScrollMs = 0; // Сбрасываем таймер, чтобы прокрутка новой песни началась мгновенно
    _currentMetadata = text; // Обновляем старый буфер для веб-интерфейса
}

// ПЛАВНЫЙ СДВИГ БУКВ (Вызывается асинхронно на Core 1)
void Radio::handleMarquee() {
    // Если радио выключено (STOP) или из интернета еще ничего не прилетело — очищаем зону и выходим
    if (!_isPlaying || _scrollText.length() == 0) {
        display.updateMetadata(""); // Метод сотрет строку пробелами
        return;
    }
    
    // Настраиваем скорость бегущей строки: один шаг (сдвиг на 1 символ) каждые 350 миллисекунд.
    // Это идеальная скорость для человеческого глаза, буквы не будут смазываться на LCD!
    if (millis() - _lastScrollMs > 350) {
        _lastScrollMs = millis();
        
        String visibleText = "";
        int textLen = _scrollText.length();
        
        // Берем кусок строки длиной 38 символов, начиная с текущего индекса сдвига
        for (int i = 0; i < 38; i++) {
            int charIndex = (_scrollIndex + i) % textLen;
            visibleText += _scrollText[charIndex];
        }
        
        // Отправляем готовые 30 символов на экран (метод выполняется на Core 1)
        display.updateMetadata(visibleText);
        
        // Шагаем индексом вперед для следующего кадра
        _scrollIndex++;
        if (_scrollIndex >= textLen) {
            _scrollIndex = 0; // Зацикливаем прокрутку по кругу
        }
    }
}
