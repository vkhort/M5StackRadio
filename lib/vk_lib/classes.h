// ============================================================
//  classes.h - Настройки SPIFFS памяти и конфигурационных данных
// ============================================================

#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
//#include <WebServer.h> // Обязательно подключаем стандартный сервер вместо AsyncWebServer

#include "config.h"

#define CONFIG_FILE "/config"  // Формат файла: чистый текст (ключ=значение)

// ============================================================
//  СТРУКТУРА ОДНОЙ РАДИОСТАНЦИИ
// ============================================================
struct StationEntry {
    String name;
    String url;
};

// ============================================================
//  КЛАСС ДЛЯ РАБОТЫ С ЭНЕРГОНЕЗАВИСИМОЙ ПАМЯТЬЮ SPIFFS
// ============================================================
class WorkSPIFFS {
public:
    // ============================================================
    //  СТРУКТУРА НАСТРОЕК (Объект данных)
    // ============================================================
    struct ConfigData {
        String ssid;
        String password;
        int currentStation; // Текущий выбранный индекс (0-9)
        int volume;         // Текущая сохраненная громкость (шкала 0-9)
        
        StationEntry* stations; // Динамический массив радиостанций в куче (Heap)
        int stationCount;       // Реальное количество загруженных станций

        ConfigData();
        ~ConfigData(); // Деструктор безопасно очистит массив stations через delete[]

        // ИСПРАВЛЕНО: Запрещаем неявное копирование структуры для защиты памяти от сбоев
        ConfigData(const ConfigData&) = delete;
        ConfigData& operator=(const ConfigData&) = delete;

        // Сервисные методы структуры данных
        void loadDefaultStations();
        String getCurrentStationName() const;
        String getCurrentStationURL() const;
        
        // ИСПРАВЛЕНО: Методы nextStation(), previousStation() и getVolumeFloat() полностью 
        // УДАЛЕНЫ отсюда, так как вся логика переключения теперь инкапсулирована в классе Radio!
    };

    // ============================================================
    //  КОНСТРУКТОР / ДЕСТРУКТОР КЛАССА SPIFFS
    // ============================================================
    WorkSPIFFS();
    ~WorkSPIFFS();

    // ============================================================
    //  БАЗОВЫЕ ОПЕРАЦИИ С ПАМЯТЬЮ
    // ============================================================
    
    // Назначение: Монтировать файловую систему, создать чистый конфиг, если файла нет
    bool begin();

    // Назначение: Загрузить настройки Wi-Fi, громкости и станций из текстового файла /config
    bool loadConfig(ConfigData& data);

    // Назначение: Сохранить текущие настройки Wi-Fi, громкость и индекс в текстовый файл /config
    bool saveConfig(const ConfigData& data);

    // Назначение: Заполнить структуру значениями по умолчанию из config.h при первом старте
    void setDefaults(ConfigData& data);

private:
    String _configFile = CONFIG_FILE; // Прямая инициализация пути к файлу
    bool _mounted;                    // Флаг успешного монтажа файловой системы
};

// ============================================================
//  КЛАСС ДЛЯ УПРАВЛЕНИЯ WI-FI (без изменений)
// ============================================================
class WiFiConnect {
public:
    enum class Mode { STA, AP, DISCONNECTED, ERROR };
    
    WiFiConnect() : _server(80) {} // Инициализация на 80 порту
    void setAPCredentials(const String& ssid, const String& password);
    bool setupWiFi(WorkSPIFFS::ConfigData& config, unsigned long timeoutMs = 15000);
    
    Mode getMode() const { return _mode; }
    bool isSTA() const { return _mode == Mode::STA; }
    bool isAP() const { return _mode == Mode::AP; }
    bool isConnected() const;
    
    String getIPAddress();
    String getAPIPAddress();
    String getCurrentSSID();
    void disconnect();
    
    // Оставляем метод публичным для тестов и ручного вызова
    void setupWebServer(); 
    void handle();
    void startAPMode();

private:
    Mode _mode;
    String targetSSID;
    String targetPassword;
    String apSSID;
    String apPassword;
    
    // === СЕРВЕР СНОВА ДОМА (ВНУТРИ КЛАССА!) ===
    // Физически создаем объект синхронного сервера на 80-м порту
    // Строку extern WebServer server из классов полностью убираем!
    // WebServer _server{80};
    AsyncWebServer _server; // Переводим сервер в асинхронный режим

    
    bool connectToWiFi(const String& ssid, const String& password, unsigned long timeoutMs);
};