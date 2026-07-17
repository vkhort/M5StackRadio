// ============================================================
//  M5StickRadio.ino - Основной файл программы (VK-Radio v2.0)
// ============================================================

#include <Arduino.h>
#include "config.h"
#include "classes.h"
#include "radio.h"
#include "display.h"
#include "html_const.h"

// === ГЛОБАЛЬНЫЕ ОБЪЕКТЫ (Размещены в оперативной памяти RAM) ===
Display display;
WorkSPIFFS myFS;
WorkSPIFFS::ConfigData config; // Та самая единственная рабочая RAM-структура
WiFiConnect wifi;
Radio radio;
String htmlWeb = ""; 
// ============================================================
//  SETUP - ИНИЦИАЛИЗАЦИЯ И СТАРТ СИСТЕМЫ
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(100);

    #if DEBUG_MODE
    Serial.println("\n======================================================");
    Serial.println("===         M5Stack Core2 Radio v3.0             ===");
    Serial.println("======================================================");
    #endif

    // 1. ИНИЦИАЛИЗАЦИЯ ПЛАТЫ ЧЕРЕЗ M5UNIFIED
    // Сама определяет Core2, настраивает чип питания, заводит PSRAM и шины данных
    auto cfg = M5.config();
    M5.begin(cfg); 

    // 2. Аппаратный запуск дисплея M5StickC Plus
    display.begin();
    
    // ИСПРАВЛЕНО: Выводим чистую стартовую заставку. 
    // На экране загорится: "WiFi: Connecting..." и "Stations: 0"
    display.showStartup("", 0); 

    // 3. Монтирование файловой системы SPIFFS и загрузка конфига
    #if DEBUG_MODE
    Serial.println("[SYSTEM] Шаг 1: Монтирование памяти SPIFFS...");
    #endif
    
    if (myFS.begin()) {
        myFS.loadConfig(config); // Загружаем все сохраненные станции и громкость в RAM
        
        #if DEBUG_MODE
        Serial.printf("[WorkSPIFFS] Успех! Загружено станций из файла: %d\n", config.stationCount);
        #endif
        
        // Мягко обновляем заставку на экране, показывая реальное количество считанных каналов
        display.showStartup("", config.stationCount);
    } else {
        #if DEBUG_MODE
        Serial.println("[WorkSPIFFS] КРИТИЧЕСКАЯ ОШИБКА: Память SPIFFS повреждена!");
        #endif
        
        display.showError("SPIFFS failed");
        while (true) { delay(100); } // Глухая блокировка, если чип Flash сломан
    }

    // 4. Подключение к Wi-Fi роутеру (В режиме STA) или запуск WiFi Manager (В режиме AP)
    #if DEBUG_MODE
    Serial.println("[SYSTEM] Шаг 2: Инициализация Wi-Fi соединения...");
    #endif
    
    // ============================================================
    //  ОДНОКРАТНАЯ СБОРКА ВЕБ-СТРАНИЦЫ ПРИ СТАРТЕ
    // ============================================================
    htmlWeb.reserve(25000); // Бронируем память один раз на всё время работы устройства
/*
    // 5. Анализируем результат подключения и запускаем аудио-задачи FreeRTOS
    if (wifi.isSTA()) {
        #if DEBUG_MODE
        Serial.println("[SYSTEM] Шаг 3: Wi-Fi подключен. Запуск аудиодвижка на Core 0...");
        #endif
        #if DEBUG_MODE
        Serial.println("[SYSTEM] Предварительная сборка веб-страницы для режима STA...");
        #endif
        htmlWeb = String(HTML_HEADER);
        htmlWeb += String(HTML_STA_BODY);
        htmlWeb += String(HTML_AP_BODY);
        htmlWeb += String(HTML_SCRIPT);
        
        // Один раз заменяем статичные маркеры
        htmlWeb.replace("{SSID}", config.ssid);
        htmlWeb.replace("{PASSWORD}", config.password);
        // Метод connectToWiFi внутри себя использует неблокирующий vTaskDelay,
        // поэтому опрос железа и вывод точек на экран пойдет плавно.
        wifi.setupWiFi(config, WIFI_TIMEOUT_MS);
        
        // Передаем нашу живую глобальную структуру config в плеер
        radio.begin(config); 
    } else {
        #if DEBUG_MODE
        Serial.println("[Radio] Аппаратный плеер отключен. Устройство работает в режиме точки доступа (AP MODE)");
        Serial.println("[Radio] Ожидание ввода настроек сети от пользователя через браузер...");
        #endif
        #if DEBUG_MODE
        Serial.println("[SYSTEM] Предварительная сборка веб-страницы для режима AP...");
        #endif
        htmlWeb = String(HTML_HEADER);
        htmlWeb += String(HTML_AP_BODY);
        htmlWeb += String(HTML_SCRIPT);
        
        htmlWeb.replace("{SSID}", config.ssid);
        htmlWeb.replace("{PASSWORD}", config.password);

        wifi.setupWiFi(config, WIFI_TIMEOUT_MS);
    }

    #if DEBUG_MODE
    Serial.println("[SYSTEM] Настройка setup() успешно завершена. Система запущена.");
    #endif
*/
}

// ============================================================
//  LOOP - СИСТЕМНЫЙ ХОЛОСТУЮ ЦИКЛ
// ============================================================
void loop() {
    // ИСПРАВЛЕНО: Полностью разгружаем системную loopTask таску.
    // Микро-пауза в 10 мс дает планировщику FreeRTOS спокойно обслуживать 
    // наши основные тяжелые задачи (AudioTask на Core 0, GyroTask на Core 1)
    vTaskDelay(pdMS_TO_TICKS(100)); 

}