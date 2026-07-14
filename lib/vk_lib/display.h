// ============================================================
//  display.h - Чистый класс управления экраном M5StickC Plus
//  Назначение: независимый позонный вывод информации радио
// ============================================================

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <M5GFX.h>

class Display {
public:
    Display();
    ~Display();

    // ---- Инициализация ----
    bool begin(); // Настройка экрана, ориентации и шрифтов (вызывается в setup)

    // ---- Базовые методы управления дисплеем ----
    void clear();                 // Очистить весь экран в черный цвет
    void setBrightness(uint8_t percent); // Установить яркость подсветки (0-100)

    // ---- СТАТИЧЕСКАЯ ИНФОРМАЦИЯ (Рисуется строго один раз при старте) ----
    void drawStaticInfo(const String& ip);

    // ---- ДИНАМИЧЕСКИЕ ОБЛАСТИ (Каждая зона обновляется независимо без мерцания) ----
    void updateStationName(const String& station); // При смене канала
    void updateVolume(int volume);                 // При изменении громкости (шкала 0-9)
    void updateMetadata(const String& metadata);   // Прилетело новое имя трека из ICY-стрима
    void drawTime(const String& time);             // Посекундное обновление часов
    void updatePlayStatus(bool isPlaying);         // Иконка PLAY (зеленая) / STOP (красная)

    // ---- СПЕЦИАЛЬНЫЕ АВАРИЙНЫЕ ЭКРАНЫ ----
    void showAPInfo(const String& apIP, const String& ssid, const String& password); // Экран WiFi Manager
    void showError(const String& message);         // Экран критической ошибки SPIFFS/Сети
    void showStartup(const String& wifiIP = "", int stationCount = 0); // Заставка при включении

    // ---- Геттеры и публичные буферы ----
    bool isInitialized() const { return _initialized; }
    int currentVolume; // Кэш-переменная для сравнения прошлого уровня звука (шкала 0-9)

private:
    // ---- Приватные поля стейт-машины графики ----
    bool _initialized;        // Флаг успешного железного старта экрана
    bool _staticDrawn;        // Флаг: нарисована ли уже фоновая рамка/заголовки
    bool _firstTimeDraw;       // Флаг первой прорисовки динамических данных
    
    String _lastMetadata;     // Кэш-буфер имени трека для защиты от мерцания перерисовки
    String _lastStation;      // Кэш-буфер имени радиостанции

    // Координаты зоны статуса PLAY / STOP
    static const int MODE_AREA_X = 176;
    static const int MODE_AREA_W = 63;
    static const int MODE_AREA_H = 35;

    // Координаты статических заголовков
    static const int HEADER_X = 40;
    static const int HEADER_Y = 5;
    static const int STATION_X = 35;
    static const int STATION_Y = 30;
    static const int IP_X = 40;
    static const int IP_Y = 100;

    // Координаты больших часов и секунд
    static const int TIME_X = 52;
    static const int TIME_Y = 60;         
    static const int SECONDS_X = 183;
    static const int SECONDS_Y = 68;      

    // Координаты бегущей строки песни
    static const int METADATA_X = 5;
    static const int METADATA_Y = 125;

    // Параметры вертикального индикатора громкости (0-9 делений)
    static const int VOLUME_BAR_X = 5;
    static const int VOLUME_BAR_Y = 5;
    static const int VOLUME_BAR_WIDTH = 15;
    static const int VOLUME_BAR_HEIGHT = 90; // По 10 пикселей на каждый шаг громкости

    // Размеры и координаты черных подложек (очистка зон перед печатью)
    static const int TIME_AREA_X = 52;
    static const int TIME_AREA_Y = 50;    
    static const int TIME_AREA_W = 120;
    static const int TIME_AREA_H = 50;    

    static const int STATION_AREA_X = 35;
    static const int STATION_AREA_Y = 25;
    static const int STATION_AREA_W = 140;
    static const int STATION_AREA_H = 35;

    static const int METADATA_AREA_X = 5;
    static const int METADATA_AREA_Y = 115;
    static const int METADATA_AREA_W = 220;
    static const int METADATA_AREA_H = 20;

    static const int VOLUME_AREA_X = 5;
    static const int VOLUME_AREA_Y = 5;
    static const int VOLUME_AREA_W = 25;
    static const int VOLUME_AREA_H = 100;
};

#endif // DISPLAY_H
