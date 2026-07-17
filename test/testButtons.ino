// ============================================================
//  TEST: M5Stack Core Buttons with FreeRTOS
//  Использует config.h для пинов и задержек
// ============================================================

#include <Arduino.h>
#include "config.h"  // Ваши пины и константы

// ============================================================
//  ЗАДАЧА ДЛЯ ОПРОСА КНОПОК (Core 1)
// ============================================================
void ButtonTask(void *pvParameters) {
    // Переменные для обработки кнопок
    static unsigned long leftPressStartMs = 0;
    static bool leftLongTriggered = false;
    
    static unsigned long midPressStartMs = 0;
    static bool midLongTriggered = false;
    
    static unsigned long rightPressStartMs = 0;
    static bool rightLongTriggered = false;
    
    // Защита от дребезга
    static unsigned long lastDebounceTime = 0;
    
    while (true) {
        unsigned long now = millis();
        
        // ---- ЗАЩИТА ОТ ДРЕБЕЗГА ----
        if (now - lastDebounceTime < 50) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }
        lastDebounceTime = now;
        
        // ---- ЧТЕНИЕ КНОПОК ----
        bool pressLeft   = (digitalRead(BTN_A_PIN) == LOW);
        bool pressMiddle = (digitalRead(BTN_B_PIN) == LOW);
        bool pressRight  = (digitalRead(BTN_C_PIN) == LOW);

        // ============================================================
        //  КНОПКА A (Левая) — BTN_A_PIN
        // ============================================================
        if (pressLeft) {
            if (leftPressStartMs == 0) {
                leftPressStartMs = now;
                leftLongTriggered = false;
            }
            if (!leftLongTriggered && (now - leftPressStartMs >= BUTTON_HOLD_TIME_MS)) {
                leftLongTriggered = true;
                Serial.println("[BtnA] ДЛИННОЕ нажатие (HOLD)");
            }
        } else {
            if (leftPressStartMs > 0) {
                unsigned long duration = now - leftPressStartMs;
                if (!leftLongTriggered && duration > 30 && duration < BUTTON_HOLD_TIME_MS) {
                    Serial.println("[BtnA] КОРОТКОЕ нажатие (CLICK)");
                }
                leftPressStartMs = 0;
            }
        }

        // ============================================================
        //  КНОПКА B (Средняя) — BTN_B_PIN
        // ============================================================
        if (pressMiddle) {
            if (midPressStartMs == 0) {
                midPressStartMs = now;
                midLongTriggered = false;
            }
            if (!midLongTriggered && (now - midPressStartMs >= BUTTON_HOLD_TIME_MS)) {
                midLongTriggered = true;
                Serial.println("[BtnB] ДЛИННОЕ нажатие (HOLD)");
            }
        } else {
            if (midPressStartMs > 0) {
                unsigned long duration = now - midPressStartMs;
                if (!midLongTriggered && duration > 30 && duration < BUTTON_HOLD_TIME_MS) {
                    Serial.println("[BtnB] КОРОТКОЕ нажатие (CLICK)");
                }
                midPressStartMs = 0;
            }
        }

        // ============================================================
        //  КНОПКА C (Правая) — BTN_C_PIN
        // ============================================================
        if (pressRight) {
            if (rightPressStartMs == 0) {
                rightPressStartMs = now;
                rightLongTriggered = false;
            }
            if (!rightLongTriggered && (now - rightPressStartMs >= BUTTON_HOLD_TIME_MS)) {
                rightLongTriggered = true;
                Serial.println("[BtnC] ДЛИННОЕ нажатие (HOLD)");
            }
        } else {
            if (rightPressStartMs > 0) {
                unsigned long duration = now - rightPressStartMs;
                if (!rightLongTriggered && duration > 30 && duration < BUTTON_HOLD_TIME_MS) {
                    Serial.println("[BtnC] КОРОТКОЕ нажатие (CLICK)");
                }
                rightPressStartMs = 0;
            }
        }

        // Небольшая задержка, чтобы не загружать процессор
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

// ============================================================
//  НАСТРОЙКА
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("\n=========================================");
    Serial.println("===  FreeRTOS BUTTON TEST (Core 1)  ===");
    Serial.println("=========================================");
    Serial.printf("BTN_A_PIN: %d\n", BTN_A_PIN);
    Serial.printf("BTN_B_PIN: %d\n", BTN_B_PIN);
    Serial.printf("BTN_C_PIN: %d\n", BTN_C_PIN);
    Serial.printf("BUTTON_HOLD_TIME_MS: %d\n", BUTTON_HOLD_TIME_MS);
    Serial.println("=========================================");
    Serial.println("Нажимайте кнопки A, B, C");
    Serial.println("Короткое нажатие < 600 мс -> CLICK");
    Serial.println("Длинное нажатие >= 600 мс -> HOLD");
    Serial.println("=========================================\n");
    
    // НАСТРОЙКА ПИНОВ КНОПОК
    pinMode(BTN_A_PIN, INPUT_PULLUP);
    pinMode(BTN_B_PIN, INPUT_PULLUP);
    pinMode(BTN_C_PIN, INPUT_PULLUP);
    
    // СОЗДАНИЕ ЗАДАЧИ НА CORE 1
    xTaskCreatePinnedToCore(
        ButtonTask,          // Функция задачи
        "ButtonTask",        // Имя задачи
        4096,                // Стек (байт)
        NULL,                // Параметры
        2,                   // Приоритет
        NULL,                // Handle задачи
        1                    // Ядро (Core 1)
    );
    
    Serial.println("[OK] Задача ButtonTask создана на Core 1");
    Serial.println("[OK] Тест запущен!\n");
}

// ============================================================
//  ГЛАВНЫЙ ЦИКЛ (пустой, т.к. всё в задаче)
// ============================================================
void loop() {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}