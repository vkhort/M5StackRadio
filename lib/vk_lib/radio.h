// ============================================================
//  radio.h - Вычищенный и оптимизированный класс управления радио
// ============================================================

#ifndef RADIO_H
#define RADIO_H

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <freertos/queue.h>
//#include "Audio.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h" // Этот файл содержит и класс AudioOutputInternalDAC
// #include "AudioOutputInternalDAC.h" // <-- КРИТИЧЕСКИЙ ДОБАВЛЕННЫЙ ИНКЛУД ДЛЯ СТРОКИ 102

#include "config.h"
#include "classes.h"      // Содержит WorkSPIFFS::ConfigData

// ============================================================
//  КОМАНДЫ ДЛЯ ОЧЕРЕДИ (Управление между ядрами)
// ============================================================
enum AudioCommand {
    CMD_NONE,
    CMD_PLAY,
    CMD_STOP,
    CMD_TOGGLE,
    CMD_NEXT,
    CMD_PREV,
    CMD_VOLUME,
    CMD_SAVE // ← Просто допишите эту строчку!
};

struct AudioMessage {
    AudioCommand command;
    int value; // Используется для передачи триггеров
};

// ============================================================
//  КЛАСС RADIO
// ============================================================
class Radio {
public:
    Radio();
    ~Radio();

    // ---- Инициализация ----
    bool begin(WorkSPIFFS::ConfigData& config); 

    // ---- Интерфейс управления (Отправка команд из Core 1 в Core 0) ----
    void play();
    void stop();
    void togglePlay();
    void nextStation();
    void previousStation();
    void setVolume(int vol);

    // ---- Задачи FreeRTOS (Методы жизненного цикла) ----
    void runAudio();          // Выполняется на Core 0 (Декодирование и I2S)
    void runControl();        // Выполняется на Core 1 (Опрос физических кнопок)
    void runNetwork();        // Выполняется на Core 1 (События Веб-сервера и NTP)

    // ---- Геттеры (Для вывода информации на дисплей и в Веб) ----
    bool isPlaying() const { return _isPlaying; }
    
    // ИСПРАВЛЕНО: Геттеры теперь читают данные напрямую из глобальной памяти config!
    int getVolume() const;
    int getCurrentStationIndex() const;
    int getStationCount() const;
    
    String getCurrentStationName() const;
    String getCurrentStationURL() const;
    String getStationName(int index) const;
    String getStationURL(int index) const;
    String getMetadata() const { return _currentMetadata; }
    String getTime() const;
    String getIP() const;

    // ---- Сервисные методы ----
    void handleButtons();
    void updateTime();
    void forceConnect();
    void setScrollText(const String& text); 
    void handleMarquee();

private:
    // ---- Время и Синхронизация ----
    time_t _lastNtpAttempt;    
    bool _ntpSuccess;          
    time_t _lastNtpSync;
    struct tm _timeInfo;

    // ---- Аудио-движок (ESP32-audioI2S) ----
    //    Audio _audio;
        // 2. ИСПРАВЛЕНО: Меняем старый объект _audio на динамические указатели новой библиотеки
    AudioGeneratorMP3        *_mp3 = nullptr;
    AudioFileSourceHTTPStream *_file = nullptr;
    AudioFileSourceBuffer     *_buff = nullptr;

    // ИСПРАВЛЕНО: Заменили AudioOutputInternalDAC на базовый класс AudioOutput
    AudioOutput              *_out = nullptr; 

    // ---- Текущее системное состояние ----
    bool _isPlaying;          // Флаг: играет сейчас радио или стоит на паузе
    String _currentMetadata;  // Буфер для названия текущей песни (бегущая строка)

    // ---- Аппаратные модули ----
    WorkSPIFFS::ConfigData* _config;  // Указатель на структуру настроек памяти

    // ---- Потокобезопасная очередь команд FreeRTOS ----
    QueueHandle_t _commandQueue;

    // ---- Экранный буфер интерфейса (Оставлен только чистый текстовый кэш) ----
    bool _screenInitialized;
    unsigned long _lastDisplayUpdate;
    String _displayStatus;
    String _displayStation;
    String _displayMetadata;
    String _displayTime;

    // ---- Внутренние приватные методы обработки ----
    void startPlaying();
    void stopPlaying();
    void processCommand(const AudioMessage& msg); // Точка разбора очереди на Core 0

    // Методы отрисовки дисплея
    void initScreen();
    void drawStatus();
    void drawStationName();
    void drawMetadata();
    void drawTime();
    void drawVolumeBar();
    void drawChannelNumber();
    void drawProgressBar(int x, int y, int w, int h, int value, int maxValue, uint16_t color);
    void clearScreen();
    void showStartScreen();
    void showError(const String& message);

    void syncNTP();

    // Друзья класса для лямбда-функций FreeRTOS в методе Radio::begin
    friend void taskAudio(void* parameter);
    friend void taskControl(void* parameter);
    friend void taskNetwork(void* parameter);
    friend void taskGyro(void* parameter); 

    String _scrollText;         // Подготовленный текст с пробелами для прокрутки
    int _scrollIndex;           // Текущий индекс сдвига букв
    unsigned long _lastScrollMs; // Таймер для шага прокрутки (без блокировки FreeRTOS)

    const int preallocateBufferSize = 16 * 1024;
    uint8_t *_bufferMem = nullptr;
};

#endif // RADIO_H
