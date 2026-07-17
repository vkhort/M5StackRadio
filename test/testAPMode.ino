#include <M5Unified.h>
#include <WiFi.h>

// Конфигурация точки доступа (Имя изменено, чтобы Windows не ругалась на старый MAC)
const char* ap_ssid = "VK_Radio_NEW";
const char* ap_pass = "12345678";

// Запускаем сервер на стандартном 80-м порту
WiFiServer server(80);

void setup() {
    // 1. Инициализация M5Unified и последовательного порта
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);
    delay(500);

    Serial.println("\n========================================");
    Serial.println("[START] Запуск теста веб-сервера в AP...");
    Serial.println("========================================");

    // 2. Сброс Wi-Fi стека и перевод в режим точки доступа
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);

    // 3. Жесткая настройка IP-адресации
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    delay(50);

    // 4. Поднятие Wi-Fi сети
    if (WiFi.softAP(ap_ssid, ap_pass)) {
        WiFi.setSleep(false); // Отключаем сон для стабильности ответов
        Serial.println("[SUCCESS] Точка доступа успешно создана!");
        Serial.print("[SUCCESS] SSID: "); Serial.println(ap_ssid);
        Serial.print("[SUCCESS] IP-адрес: "); Serial.println(WiFi.softAPIP());
    } else {
        Serial.println("[ERROR] Не удалось запустить softAP!");
    }

    // 5. Старт веб-сервера
    server.begin();
    Serial.println("[SUCCESS] Веб-сервер успешно запущен на порту 80.");
    Serial.println("========================================");
}

void loop() {
    // Ждем подключения клиента (браузера с ПК)
    WiFiClient client = server.available();
    
    if (client) {
        Serial.println("[CONNECT] Браузер отправил запрос.");
        String currentLine = "";
        
        // Читаем HTTP-запрос от браузера, пока он подключен
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                if (c == '\n') {
                    // Если строка пустая — это конец HTTP-заголовка запроса
                    if (currentLine.length() == 0) {
                        // Отправляем стандартный HTTP-ответ успешной загрузки страницы (код 200)
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html; charset=utf-8");
                        client.println("Connection: close");
                        client.println(); // Обязательный пустой разрыв между заголовками и телом
                        
                        // ВЫВОД ЗАДАННОЙ СТРОКИ НА ЭКРАН ПК В БРАУЗЕРЕ
                        client.println("<h1>AP-mode working</h1>");
                        break;
                    } else {
                        currentLine = "";
                    }
                } else if (c != '\r') {
                    currentLine += c;
                }
            }
        }
        // Закрываем соединение, чтобы браузер завершил отрисовку
        client.stop();
        Serial.println("[DISCONNECT] Сессия с браузером закрыта.");
    }
    
    delay(10); // Небольшая пауза в цикле loop
}
