/*
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h> // Используем стандартный, стабильный синхронный сервер ядра

// Подключаем ваш оригинальный файл констант (из которого берутся HEADER, STA_BODY и т.д.)
#include "html_const.h"

// Создаем объект сервера на стандартном 80-м порту
WebServer server(80);

// Глобальная переменная, за поведением которой мы следим
String htmlWeb = "";

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n=============================================");
    Serial.println("===   ЛИНЕЙНЫЙ ТЕСТ ВЫДАЧИ СТРАНИЦЫ 22 КБ   ===");
    Serial.println("=============================================");

    // 1. Инициализация базового железа Core2
    auto cfg = M5.config();
    M5.begin(cfg);

    // 2. Включаем питание встроенного усилителя звука (Пин 22)
    pinMode(22, OUTPUT);
    digitalWrite(22, HIGH);

    // 3. ПРИНУДИТЕЛЬНОЕ ПОДКЛЮЧЕНИЕ К ВАШЕМУ ДОМАШНЕМУ Wi-Fi (STA)
    // Впишите сюда имя и пароль вашей реальной домашней Wi-Fi сети!
    const char* mySSID = "MGTS_GPON_9135";
    const char* myPASS = "AWJWFMNT";

    Serial.printf("[WiFi] Пробуем подключиться к роутеру: %s ...\n", mySSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(mySSID, myPASS);

    // Ждем подключения не более 10 секунд
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] УСПЕХ! Подключено. IP-адрес платы: %s\n", WiFi.localIP().toString().c_str());
        
        // === 4. СБОРКА СТРАНИЦЫ ИЗ ВАШИХ ОРИГИНАЛЬНЫХ 4 КОНСТАНТ ===
        // Мы используем макрос F(), чтобы гарантировать правильное вычитывание из PROGMEM флэша
        htmlWeb = String(F(HTML_HEADER));
        htmlWeb += String(F(HTML_STA_BODY));
        htmlWeb += String(F(HTML_AP_BODY));
        htmlWeb += String(F(HTML_SCRIPT));
        
        // Подставляем маркеры сети для проверки
        htmlWeb.replace("{SSID}", mySSID);
        htmlWeb.replace("{PASSWORD}", myPASS);

        Serial.printf("[SYSTEM] Строка htmlWeb собрана в RAM. Размер: %d байт\n", htmlWeb.length());
        
        // === 5. НАСТРОЙКА СИНХРОННОГО МАРШРУТА ВЫДАЧИ ===
        server.on("/", HTTP_GET, [](){
            Serial.println("\n[Server] Клик в браузере Chrome на ПК!");
            Serial.printf("[Server] Длина глобальной строки htmlWeb прямо сейчас: %d байт\n", htmlWeb.length());
            
            // Запрещаем кэширование пульта на уровне HTTP заголовков
            server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
            server.sendHeader("Pragma", "no-cache");
            server.sendHeader("Expires", "-1");
            
            // Отправляем всю 22-килобайтную строку. 
            // Стандартный WebServer ядра делает это поточно, не перегружая память!
            server.send(200, "text/html", htmlWeb);
            Serial.println("[Server] Вся страница успешно улетела в сеть.");
        });

        // Заглушка для иконки сайта, чтобы убрать ошибку 500/404 из консоли Chrome
        server.on("/favicon.ico", HTTP_GET, [](){
            server.send(204, "text/plain", ""); 
        });

        // Запускаем веб-сервер
        server.begin();
        Serial.println("[SYSTEM] Стандартный WebServer успешно запущен.");
    } else {
        Serial.println("\n[WiFi] Не удалось подключиться к роутеру. Проверьте пароль!");
        while(true) { delay(100); }
    }

    Serial.println("[SYSTEM] Настройка setup() успешно завершена. Ждем запросов в loop()...");
}

void loop() {
    // В линейном скетче мы обязаны непрерывно отдавать процессору 
    // команды на обслуживание входящих сетевых клиентов
    server.handleClient();
    delay(2); // Микропауза для стабильности Wi-Fi стека
}
*/


#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <AsyncTCP.h>

// Подключаем ваши оригинальные файлы проекта
#include "classes.h"
#include "radio.h"
#include "display.h"
#include "html_const.h"

// Объявляем глобальные переменные, как в вашем основном коде
WorkSPIFFS myFS;
WorkSPIFFS::ConfigData config;
WiFiConnect wifi;
Radio radio;
Display display;
String htmlWeb = ""; 

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n======================================================");
    Serial.println("===     СТЕРИЛЬНЫЙ ТЕСТ ВЕБ-СТРАНИЦЫ В РЕЖИМЕ STA   ===");
    Serial.println("======================================================");

    // 1. Инициализация железа через M5Unified
    auto cfg = M5.config();
    M5.begin(cfg);

    // 2. Включаем питание встроенного усилителя звука Core2 (Пин 22)
    pinMode(PIN_AMP_SPK_EN, OUTPUT);
    digitalWrite(PIN_AMP_SPK_EN, HIGH);

    // 3. Инициализация вашего оригинального класса дисплея
    display.begin();
    display.showStartup("TEST CONNECTING...", 0);

    // === ЖЕСТКАЯ НАСТРОЙКА ДЛЯ ПРИНУДИТЕЛЬНОГО РЕЖИМА STA ===
    // Впишите сюда имя и пароль вашей реальной домашней Wi-Fi сети!
    const char* mySSID = "MGTS_GPON_9135";
    const char* myPASS = "AWJWFMNT";

    Serial.printf("[WiFi-Test] Пробуем подключиться к роутеру: %s ...\n", mySSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(mySSID, myPASS);

    // Ждем подключения к домашнему роутеру
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi-Test] УСПЕХ! IP-адрес платы: %s\n", WiFi.localIP().toString().c_str());

        // === ШАГ 4: СБОРКА СТРАНИЦЫ ИЗ ВАШИХ ОРИГИНАЛЬНЫХ 4 КОНСТАНТ ===
        htmlWeb = String(HTML_HEADER);
        htmlWeb += String(HTML_STA_BODY);
        htmlWeb += String(HTML_AP_BODY);
        htmlWeb += String(HTML_SCRIPT);
        
        // Подставляем маркеры сети
        htmlWeb.replace("{SSID}", mySSID);
        htmlWeb.replace("{PASSWORD}", myPASS);

        Serial.printf("[SYSTEM-Test] Строка htmlWeb собрана в RAM. Размер: %d байт\n", htmlWeb.length());

        // === ШАГ 5: СТАРТ СЕТЕВОГО СТЕКА ЧЕРЕЗ РОДНОЙ МЕТОД КЛАССА ===
        // Загружаем имя и пароль в конфиг, чтобы класс WiFiConnect знал их
        config.ssid = mySSID;
        config.password = myPASS;
        config.volume = 5;
        config.stationCount = 1;

        // Вызываем ваш стандартный публичный метод! Он сам внутри себя 
        // безопасно настроит маршруты и запустит приватный _server.begin()
        wifi.setupWiFi(config); 
        Serial.println("[SYSTEM-Test] Веб-сервер успешно запущен через метод класса.");

        // === ШАГ 6: БЕЗУСЛОВНЫЙ ЗАПУСК ВАШИХ 4 ЗАДАЧ FREERTOS ===
        // Рождаем ваши оригинальные потоки runAudio, runControl, runNetwork, runGyroJoystick
        radio.begin(config);

    } else {
        Serial.println("\n[WiFi-Test] Не удалось подключиться к роутеру. Проверьте пароль!");
        while(true) { delay(100); }
    }

    Serial.println("[SYSTEM-Test] Настройка setup() завершена. Система запущена.");
}

void loop() {
    // Главный поток Arduino просто отдыхает, вся работа идет в ваших задачах FreeRTOS
    vTaskDelay(portMAX_DELAY);
}