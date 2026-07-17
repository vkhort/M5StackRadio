#include <M5Unified.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

// Данные твоей домашней сети
const char* host_ssid = "MGTS_GPON_9135";
const char* host_pass = "AWJWFMNT";

// Объявляем асинхронный сервер на стандартном 80 порту
AsyncWebServer server(80);

void setup() {
    // 1. Инициализация железа M5Stack через M5Unified
    auto cfg = M5.config();
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(500);
    Serial.println("\n========================================");
    Serial.println("[TEST] Старт асинхронного STA-теста...");
    Serial.println("========================================");

    // 2. Сброс предыдущих подключений Wi-Fi
    WiFi.disconnect(true, true);
    delay(200);
    WiFi.mode(WIFI_STA);
    delay(200);

    // 3. Подключение к домашнему роутеру
    Serial.printf("[TEST] Пробуем подключиться к: %s...\n", host_ssid);
    WiFi.begin(host_ssid, host_pass);

    // Ждем подключения не более 15 секунд
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 30) {
        delay(500);
        Serial.print(".");
        timeout++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        WiFi.setSleep(false); // Отключаем энергосбережение антенны
        Serial.println("\n========================================");
        Serial.println("[SUCCESS] Wi-Fi успешно подключен!");
        Serial.print("[SUCCESS] Системный IP: ");
        Serial.println(WiFi.localIP());
        Serial.println("========================================");
    } else {
        Serial.println("\n[ERROR] Не удалось подключиться к роутеру!");
        while(true) { delay(1000); } // Глухая блокировка при ошибке Wi-Fi
    }

    // 4. НАСТРОЙКА АСИНХРОННОГО ЭНДПОИНТА КОРИНЯ "/"
    // Используем твой эталонный фикс по передаче строки напрямую из памяти RAM
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("[Web Core1] Запрос из Chrome получен. Отдаем ответ...");
        
        // Наша тестовая строка
        static const String testHtml = "<h1>Connect! Asinchrone mode</h1>";
        
        // Отправляем напрямую указатель на массив байт в оперативной памяти (без копирования!)
        AsyncWebServerResponse *response = request->beginResponse(
            200, 
            "text/html", 
            (const uint8_t*)testHtml.c_str(), 
            testHtml.length()
        );
        
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });

    // 5. Запуск сервера в фоновом режиме прерываний
    server.begin();
    Serial.println("[SUCCESS] Асинхронный сервер запущен в фоне.");
    Serial.println("========================================");
}

void loop() {
    // В асинхронном режиме loop абсолютно пустой и разгружен!
    // Никаких опросов handleClient() или wifi.handle() тут быть не должно.
    vTaskDelay(pdMS_TO_TICKS(100)); 
}
