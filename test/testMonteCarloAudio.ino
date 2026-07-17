// Подкрученная версия Google
// --- Подключаем необходимые библиотеки ---
#include <M5Unified.h>               // Главная библиотека M5Stack, инициализирует дисплей, кнопки и аудио
#include <WiFi.h>
#include <AudioFileSourceHTTPStream.h> // Для потоковой передачи по HTTP
#include <AudioFileSourceBuffer.h>    // Для буферизации данных
#include <AudioGeneratorMP3.h>        // MP3-декодер
#include <AudioOutputI2S.h>           // Вывод звука через I2S / Встроенный ЦАП

// --- Настройки Wi-Fi ---
const char* ssid = "MGTS_GPON_9135";
const char* password = "AWJWFMNT";
const char* streamURL = "http://montecarlo.hostingradio.ru:80/montecarlo128.mp3";

// --- Объекты аудиоплеера ---
AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceHTTPStream *file = nullptr;
AudioFileSourceBuffer *buff = nullptr;
AudioOutputI2S *out = nullptr;

// Размер буфера предзагрузки во внутренней SRAM (16 Кб — спасение для плат без PSRAM)
const int preallocateBufferSize = 16 * 1024;
uint8_t *bufferMem = nullptr;

// --- Функция подключения к Wi-Fi ---
bool connectToWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Подключение к Wi-Fi");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Wi-Fi подключен! IP: " + WiFi.localIP().toString());
        return true;
    } else {
        Serial.println("Ошибка подключения Wi-Fi!");
        return false;
    }
}

// --- НАСТРОЙКА УСТРОЙСТВА ---
void setup() {
    // 1. Аппаратный запуск M5Unified
    auto cfg = M5.config();
    // Принудительно отключаем внутренний спикер на уровне M5Unified,
    // чтобы системный микшер полностью освободил 25-й пин ЦАП для библиотеки ESP8266Audio
    cfg.internal_spk = false; 
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n========================================");
    Serial.println("M5STACK RADIO - SUCCESS CONFIG");
    Serial.println("========================================");

    // 2. Подключаем Wi-Fi
    if (!connectToWiFi()) {
        return;
    }
    
    // Жестко отключаем энергосбережение антенны, чтобы музыка не заикалась
    WiFi.setSleep(false);

    // 3. Выделяем чистую память под интернет-буфер
    bufferMem = (uint8_t *)malloc(preallocateBufferSize);

    // 4. НАСТРОЙКА АУДИОВЫХОДА НА ВСТРОЕННЫЙ АНАЛОГОВЫЙ ЦАП (GPIO25)
    // Передаем 0 (внутренний порт) и маркер INTERNAL_DAC (число 1), 
    // чтобы процессор генерировал чистую звуковую синусоиду прямо на встроенный динамик!
    out = new AudioOutputI2S(0, 1); 
    
    // Включаем моно-режим для экономии RAM буферов DMA
    out->SetOutputModeMono(true);
    
    // Выкручиваем громкость декодера (0.5 для уверенного и чистого звука на старте)
    out->SetGain(0.5); 

    // 5. ИНИЦИАЛИЗАЦИЯ И ЗАПУСК ДЕКОДЕРА
    Serial.println("[Audio] Открытие HTTP потока Монте-Карло...");
    file = new AudioFileSourceHTTPStream();
    file->SetReconnect(3, 3000); // Автопереподключение при обрыве связи
    
    if (!file->open(streamURL)) {
        Serial.println("✗ Ошибка открытия URL адреса станции!");
        return;
    }

    buff = new AudioFileSourceBuffer(file, bufferMem, preallocateBufferSize);
    mp3 = new AudioGeneratorMP3();
    
    // Пинаем аудио-движок
    if (mp3->begin(buff, out)) {
        Serial.println("\n╔═══════════════════════════════════╗");
        Serial.println("║ ♪ РАДИО МОНТЕ-КАРЛО ЗАИГРАЛО! ♪   ║");
        Serial.println("╚═══════════════════════════════════╝\n");
    }
}

// --- ГЛАВНЫЙ ЦИКЛ ВОСПРОИЗВЕДЕНИЯ ---
void loop() {
    // В бесконечном цикле непрерывно скачиваем чанки MP3 и декодируем их в ЦАП
    if (mp3 && mp3->isRunning()) {
        if (!mp3->loop()) {
            mp3->stop();
            Serial.println("Поток прерван. Перезапуск...");
            delay(2000);
            mp3->begin(buff, out);
        }
    } else {
        delay(100);
    }

    // Опрос системных событий M5Stack
    M5.update();
}

/*
// Версия DeepSeek
// --- Подключаем необходимые библиотеки ---
#include <M5Unified.h>                 // Главная библиотека M5Stack, инициализирует дисплей, кнопки и аудио
#include <WiFi.h>
#include <AudioFileSourceHTTPStream.h> // Для потоковой передачи по HTTP
#include <AudioFileSourceBuffer.h>     // Для буферизации данных
#include <AudioGeneratorMP3.h>         // MP3-декодер
#include <AudioOutputI2S.h>            // Вывод звука через I2S

// --- Настройки Wi-Fi ---
const char* ssid = "MGTS_GPON_9135";
const char* password = "AWJWFMNT";
const char* streamURL = "http://montecarlo.hostingradio.ru:80/montecarlo128.mp3";

// --- Объекты аудиоплеера ---
AudioGeneratorMP3 *mp3;
AudioFileSourceHTTPStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;

// --- Функция подключения к Wi-Fi ---
bool connectToWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Подключение к Wi-Fi");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Wi-Fi подключен! IP: " + WiFi.localIP().toString());
        return true;
    } else {
        Serial.println("Ошибка подключения Wi-Fi!");
        return false;
    }
}


// --- Функция инициализации и запуска воспроизведения ---
bool startPlayback() {
    // Освобождаем память от предыдущих объектов, если они были
    if (mp3) { mp3->stop(); delete mp3; mp3 = nullptr; }
    if (buff) { delete buff; buff = nullptr; }
    if (file) { delete file; file = nullptr; }

    // 1. Создаем и настраиваем звуковой выход.
    out = new AudioOutputI2S();
    // !!! КЛЮЧЕВОЙ МОМЕНТ ДЛЯ M5STACK CORE (BASE) !!!
    // Мы используем встроенный ЦАП ESP32, который работает через I2S-выход.
    // Для этого нужно указать второй параметр INTERNAL_DAC.
    // Звук пойдет на стандартный для M5Stack Core вывод GPIO25 [citation:7][citation:11].
    out = new AudioOutputI2S(I2S_NUM_0, AudioOutputI2S::INTERNAL_DAC);
    out->SetOutputModeMono(true);
    out->SetGain(0.5); // Громкость (от 0.0 до 1.0)

    // 2. Создаем источник данных из HTTP-потока.
    file = new AudioFileSourceHTTPStream();
    if (!file->open(streamURL)) {
        Serial.println("Ошибка открытия URL!");
        return false;
    }

    // 3. Создаем буфер для более стабильного воспроизведения.
    buff = new AudioFileSourceBuffer(file, 4096); // 4KB буфер

    // 4. Создаем и запускаем MP3-декодер.
    mp3 = new AudioGeneratorMP3();
    if (!mp3->begin(buff, out)) {
        Serial.println("Ошибка запуска MP3-декодера!");
        return false;
    }

    Serial.println("Воспроизведение запущено!");
    return true;
}

// --- Setup ---
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Инициализация M5Stack. Это включает динамик (если нужно) и настраивает GPIO.
    M5.begin();
    // Для некоторых моделей может потребоваться явно включить питание динамика:
    // M5.Axp.SetSpkEnable(true); // Если у вас M5Core2, раскомментируйте
    M5.Speaker.setVolume(128); // Настройка громкости для системного динамика M5Unified (не используется, но на всякий случай)
    
    M5.Lcd.setTextSize(2);
    M5.Lcd.println("M5Stack Radio");

    if (!connectToWiFi()) {
        M5.Lcd.println("WiFi Error!");
        return;
    }

    if (!startPlayback()) {
        M5.Lcd.println("Playback Error!");
    }
}

// --- Loop ---
void loop() {
    // Обновляем состояние M5Stack (кнопки, дисплей)
    M5.update();

    // Проверяем статус Wi-Fi и переподключаемся при необходимости
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi потерян. Переподключение...");
        connectToWiFi();
        // Если соединение восстановлено, но воспроизведение остановилось
        if (WiFi.status() == WL_CONNECTED && mp3 != nullptr && !mp3->isRunning()) {
            startPlayback();
        }
    }

    // Основной цикл воспроизведения
    if (mp3 != nullptr && mp3->isRunning()) {
        if (!mp3->loop()) {
            // Воспроизведение остановилось (конец потока или ошибка)
            Serial.println("Воспроизведение остановлено. Перезапуск через 3 секунды...");
            delay(3000);
            startPlayback(); // Пробуем перезапустить
        }
    } else if (mp3 != nullptr && WiFi.status() == WL_CONNECTED) {
        // Если воспроизведение не запущено, но Wi-Fi есть, запускаем его
        delay(5000);
        startPlayback();
    }
    
    delay(1); // Короткая задержка для стабильности
}
*/