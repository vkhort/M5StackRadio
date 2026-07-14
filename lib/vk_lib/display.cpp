// ============================================================
//  display.cpp - Реализация класса Display
//  Все координаты и размеры элементов экрана хранятся здесь
//  Каждый метод обновляет только свою область
// ============================================================

#include "display.h"
#include "config.h"
#include <M5Unified.h>

#define Lcd Display

// ============================================================
//  КОНСТРУКТОР / ДЕСТРУКТОР
// ============================================================
Display::Display()
    : _initialized(false)
    , _staticDrawn(false)
    , _firstTimeDraw(true) // Инициализируем флаг первой прорисовки динамики
    , currentVolume(-1)    // Кэш-буфер громкости (шкала 0-9)
    , _lastMetadata("")
    , _lastStation("")
{
}

Display::~Display() {
    // Ничего не делаем
}

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ ДИСПЛЕЯ (Вызывается в setup)
// ============================================================
bool Display::begin() {
    // 1. Сбрасываем графические флаги под экран Core2
    _initialized = true;
    _staticDrawn = false;
    _firstTimeDraw = true; // Сбрасываем флаг, чтобы динамика перерисовалась с нуля

    // === КРИТИЧЕСКИЙ ФИКС ДЛЯ СЛЕПОЙ ПЛАТЫ CORE2 ===
    // Задаем альбомный поворот экрана для Core2 (горизонтально)
    M5.Display.setRotation(1); 
    
    // Принудительно заливаем виртуальный экран в памяти черным цветом!
    // Это разметит холст в оперативной памяти и уберет зависание шины SPI.
    M5.Display.fillScreen(0x0000); // 0x0000 - Черный цвет (BLACK)

    #if DEBUG_MODE
    Serial.println("[Display v3.0] Виртуальный холст Core2 успешно подготовлен в RAM.");
    #endif

    return true;
}

void Display::clear() {
    if (!_initialized) return;
    M5.Lcd.fillScreen(BLACK);
    _staticDrawn = false;
}

void Display::setBrightness(uint8_t percent) {
    if (!_initialized) return;
    percent = constrain(percent, 0, 100);
    M5.Display.setBrightness(percent);
}

// ============================================================
//  СТАТИЧЕСКАЯ ИНФОРМАЦИЯ (Рисуется строго один раз при старте)
// ============================================================
void Display::drawStaticInfo(const String& ip) {
    if (!_initialized) return;
    if (_staticDrawn) return;
    
    // Мягко очищаем экран перед первой прорисовкой интерфейса
    clear();
    
    // === 1. ГЛАВНЫЙ ЗАГОЛОВОК МАГНИТОЛЫ ===
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.setCursor(HEADER_X, HEADER_Y);
    M5.Lcd.print("M5Stick Radio");
    
    // === 2. СИСТЕМНЫЙ IP-АДРЕС В СЕТИ ДОМА ===
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.setCursor(IP_X, IP_Y);
    M5.Lcd.print("IP: " + ip);
    
    _staticDrawn = true;
    
    #if DEBUG_MODE
    Serial.println("[Display] Статический фон и заголовки успешно отрисованы");
    #endif
}
// ============================================================
//  ОБРАБОТКА ДИНАМИЧЕСКИХ ЗОН ЭКРАНА (Выполняется на Core 1)
// ============================================================

// ============================================================
//  updateStationName() - Обновление названия радиостанции
// ============================================================
void Display::updateStationName(const String& station) {
    if (!_initialized || !_staticDrawn) return;
    if (station == _lastStation) return;
    
    // Включаем текстовый режим "Текст + Черный фон". Новые буквы сами затрут старые пиксели!
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK); 
    M5.Lcd.setCursor(STATION_X, STATION_Y);
    
    // Обрезаем название станции под физические размеры выделенной зоны (макс 11 символов)
    String displayName = station;
    if (displayName.length() > 11) {
        displayName = displayName.substring(0, 9) + "..";
    }
    
    // Дописываем пробелы в конец строки, чтобы полностью стереть остатки прошлого длинного имени 
    // БЕЗ вызова мерцающего метода fillRect(BLACK)! Это секрет идеальной плавности экрана M5Stick.
    while (displayName.length() < 11) {
        displayName += " ";
    }
    
    M5.Lcd.print(displayName);
    _lastStation = station;
}

// ============================================================
//  updateVolume() - Вертикальный индикатор громкости (Шкала 0-9)
// ============================================================
void Display::updateVolume(int volume) {
    if (!_initialized) return;   
    if (volume == currentVolume) return;
    
    // Ограничиваем входящие значения в рамках нашей жесткой шкалы 0-9
    volume = constrain(volume, 0, 9);
    
    // Рассчитываем высоту заполнения. VOLUME_BAR_HEIGHT равен 90 пикселей.
    // Для шкалы 0-9 это дает идеально ровные 10 пикселей высоты на каждый шаг звука!
    int fillHeight = (volume * VOLUME_BAR_HEIGHT) / 9;
    
    // Очищаем фоновый прямоугольник полоски темным цветом
    M5.Lcd.fillRect(VOLUME_BAR_X, VOLUME_BAR_Y, VOLUME_BAR_WIDTH, VOLUME_BAR_HEIGHT, TFT_DARKGREY);
    
    // Отрисовываем уровень заполнения снизу вверх (цветовая гамма адаптирована под шкалу 0-9)
    if (fillHeight > 0) {
        uint16_t color;
        if (volume <= 2) {         // От 0 до 2 — безопасный тихий красный
            color = TFT_RED;
        } else if (volume <= 6) {  // От 3 до 6 — комфортный желтый
            color = TFT_YELLOW;
        } else {                   // От 7 до 9 — громкий зеленый
            color = TFT_GREEN;
        }
        
        // Математика рисования прямоугольника снизу вверх
        M5.Lcd.fillRect(VOLUME_BAR_X, VOLUME_BAR_Y + VOLUME_BAR_HEIGHT - fillHeight, 
                        VOLUME_BAR_WIDTH, fillHeight, color);
    }
    
    // Восстанавливаем внешнюю аккуратную белую рамку вокруг шкалы громкости
    M5.Lcd.drawRect(VOLUME_BAR_X, VOLUME_BAR_Y, VOLUME_BAR_WIDTH, VOLUME_BAR_HEIGHT, TFT_WHITE);
    
    // Вывод цифрового значения уровня под полоской индикатора
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK); // Текст на черной подложке
    M5.Lcd.setCursor(VOLUME_BAR_X + 4, VOLUME_BAR_Y + VOLUME_BAR_HEIGHT + 4);
    M5.Lcd.print(String(volume));
    
    currentVolume = volume;
}

// ============================================================
//  updateMetadata() - Отрисовка названия текущего трека
// ============================================================
void Display::updateMetadata(const String& metadata) {
    if (!_initialized || !_staticDrawn) return;
    if (metadata == _lastMetadata) return;
    
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK); // Текст на черной подложке стирает сам себя
    M5.Lcd.setCursor(METADATA_X, METADATA_Y);
    
    String displayMeta = metadata;
    
    // ИСПРАВЛЕНО ДЛЯ МАКСИМАЛЬНОЙ ШИРИНЫ: Обрезаем строку под 38 символов мелкого шрифта
    if (displayMeta.length() > 38) {
        displayMeta = displayMeta.substring(0, 36) + "..";
    }
    
    // ИСПРАВЛЕНО ДЛЯ МАКСИМАЛЬНОЙ ШИРИНЫ: Добиваем пробелами ровно до 38 символов,
    // чтобы черная подложка гарантированно затирала буквы до самого правого края экрана!
    while (displayMeta.length() < 38) {
        displayMeta += " ";
    }
    
    M5.Lcd.print(displayMeta);
    _lastMetadata = metadata;
}

// ============================================================
//  ПУБЛИЧНЫЕ МЕТОДЫ ОТРИСОВКИ ДИНАМИЧЕСКИХ ЗОН (Core 1)
// ============================================================

void Display::drawTime(const String& time) {
    if (!_initialized || !_staticDrawn) return;

    // Быстро и безопасно вытаскиваем разряды времени из строки "HH:MM:SS"
    int h = time.substring(0, 2).toInt();
    int m = time.substring(3, 5).toInt();
    int s = time.substring(6, 8).toInt();

    // Статические кэш-буферы для отслеживания РЕАЛЬНЫХ изменений времени.
    // Это страхует от повторной перерисовки одних и тех же цифр при случайных вызовах!
    static int lastH = -1;
    static int lastM = -1;
    static int lastS = -1;

    // === ПЕРВЫЙ СТАРТОВЫЙ КАДР ЧАСОВ — прорисовываем сетку ===
    if (_firstTimeDraw) {
        _firstTimeDraw = false;
        
        // Включаем режим "Шрифт + Черный фон". Буквы сами затирают пиксели под собой.
        M5.Lcd.setTextSize(4);
        M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
        
        // Физически выводим Часы
        M5.Lcd.setCursor(TIME_X, TIME_Y);
        M5.Lcd.printf("%02d", h);
        
        // Выводим разделительное двоеточие
        M5.Lcd.setCursor(TIME_X + 48, TIME_Y);
        M5.Lcd.print(":");
        
        // Выводим Минуты
        M5.Lcd.setCursor(TIME_X + 72, TIME_Y);
        M5.Lcd.printf("%02d", m);
        
        // Переключаемся на шрифт поменьше для Секунд
        M5.Lcd.setTextSize(3);
        M5.Lcd.setCursor(SECONDS_X, SECONDS_Y);
        M5.Lcd.printf("%02d", s);

        // Фиксируем стартовые стейты, чтобы не перерисовывать их на следующей итерации
        lastH = h; lastM = m; lastS = s;
        return;
    }

    // === 1. СЕКУНДЫ (Обновляются каждую секунду, если значение изменилось) ===
    if (s != lastS) {
        // ИСПРАВЛЕНО: Тяжелый и мерцающий fillRect(BLACK) ПОЛНОСТЬЮ УДАЛЕН.
        // Новые цифры секунд будут аппаратно и идеально плавно затирать старые!
        M5.Lcd.setTextSize(3);
        M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
        M5.Lcd.setCursor(SECONDS_X, SECONDS_Y);
        M5.Lcd.printf("%02d", s);
        lastS = s;
    }
    
    // === 2. МИНУТЫ (Обновляются строго раз в минуту, убирая лишнюю нагрузку) ===
    if (m != lastM) {
        M5.Lcd.setTextSize(4);
        M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
        M5.Lcd.setCursor(TIME_X + 72, TIME_Y);
        M5.Lcd.printf("%02d", m);
        lastM = m;
    }
    
    // === 3. ЧАСЫ (Обновляются строго раз в час) ===
    if (h != lastH) {
        M5.Lcd.setTextSize(4);
        M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
        M5.Lcd.setCursor(TIME_X, TIME_Y);
        M5.Lcd.printf("%02d", h);
        lastH = h;
    }
}
// ============================================================
//  ВНУТРЕННИЕ ИНСТРУМЕНТЫ ОТРИСОВКИ ГРАФИКИ (Core 1)
// ============================================================

// ============================================================
//  СТАРЫЕ МЕТОДЫ drawVolumeBar() И drawMetadata() УДАЛЕНЫ ОТСЮДА!
// ============================================================
// Их код перенесен напрямую в публичные функцииupdateVolume() 
// и updateMetadata(), как мы зафиксировали в файле display.h, 
// чтобы не плодить дублирующие вызовы.
// ============================================================
//  СПЕЦИАЛЬНЫЕ И АВАРИЙНЫЕ ЭКРАНЫ (Выполняются на Core 1)
// ============================================================

// ============================================================
//  showAPInfo() - Экран аварийной точки доступа WiFi Manager
// ============================================================
void Display::showAPInfo(const String& apIP, const String& ssid, const String& password) {
    if (!_initialized) return; // Быстрый выход, экран уже запущен в setup()
    
    clear(); // Полная очистка экрана
    _staticDrawn = false; // Сбрасываем флаг статики, так как старый фон радио стерт
    
    #if DEBUG_MODE
    Serial.println("========================================");
    Serial.println("[Display Core1] Аварийный режим AP активирован");
    Serial.println("  SSID: " + ssid);
    Serial.println("  Password: " + password);
    Serial.println("  IP: " + apIP);
    Serial.println("========================================");
    #endif
    
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("AP MODE");
    
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.print("SSID: ");
    M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Lcd.println(ssid);
    
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(10, 70);
    M5.Lcd.print("PASS: ");
    M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Lcd.println(password);
    
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(10, 90);
    M5.Lcd.print("IP:   ");
    M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Lcd.println(apIP);
    
    M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Lcd.setCursor(10, 120);
    M5.Lcd.println("Connect to configure");
}

// ============================================================
//  showError() - Асинхронный неблокирующий экран ошибки
// ============================================================
void Display::showError(const String& message) {
    if (!_initialized) return;
    
    clear();
    _staticDrawn = false;
    
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);
    M5.Lcd.setCursor(10, 20);
    M5.Lcd.println("ERROR");
    
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(10, 60);
    M5.Lcd.println(message);
    
    // КРИТИЧЕСКИЙ ФИКС: УДАЛЕН блокирующий delay(2000)!
    // Управление мгновенно возвращается планировщику задач.
    // Задержку выведет сам вызывающий цикл через безопасный vTaskDelay.
}

// ============================================================
//  showStartup() - Стартовая заставка при включении радио
// ============================================================
void Display::showStartup(const String& wifiIP, int stationCount) {
    if (!_initialized) return;
    
    clear();
    _staticDrawn = false;
    
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Lcd.setCursor(10, 20);
    M5.Lcd.println("M5Stick");
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.println("Radio v2.0");
    
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(10, 85);
    M5.Lcd.print("WiFi: ");
    M5.Lcd.print(wifiIP.isEmpty() ? "Connecting..." : wifiIP);
    
    M5.Lcd.setCursor(10, 100);
    M5.Lcd.print("Stations: ");
    M5.Lcd.print(String(stationCount));
}

// ============================================================
//  updatePlayStatus() - Отрисовка маленькой зоны PLAY / STOP
// ============================================================
void Display::updatePlayStatus(bool isPlaying) {
    if (!_initialized || !_staticDrawn) return;
    
    // ИСПРАВЛЕНО: Стираем строго отведённую область статуса, используя 
    // задекларированные в display.h константы MODE_AREA_X и MODE_AREA_H.
    M5.Lcd.fillRect(MODE_AREA_X, STATION_Y, MODE_AREA_W, MODE_AREA_H, BLACK);
    
    M5.Lcd.setTextSize(2);
    // Включаем подложку TFT_BLACK
    if (isPlaying) {
        M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Lcd.setCursor(MODE_AREA_X, STATION_Y + 5);
        M5.Lcd.print("PLAY");
    } else {
        M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);
        M5.Lcd.setCursor(MODE_AREA_X, STATION_Y + 5);
        M5.Lcd.print("STOP");
    }
}
