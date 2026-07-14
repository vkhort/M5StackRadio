#ifndef CONFIG_H
#define CONFIG_H

#define AUDIO_TASK_DELAY_MS 1  // Микро-пауза в 1 мс для разгрузки ядра Core 0

// ============================================================
//  НАСТРОЙКИ WI-FI ПО УМОЛЧАНИЮ
//  (используются, если нет сохраненного конфига)
// ============================================================
#define DEFAULT_SSID     "VK_Radio"
#define DEFAULT_PASSWORD "12345678"

// ============================================================
//  НАСТРОЙКИ РАДИО ПО УМОЛЧАНИЮ
// ============================================================
#define DEFAULT_VOLUME      5       // Громкость (0-10)
#define DEFAULT_STATION     0       // Индекс первой станции
#define MAX_STATIONS        10      // Максимальное количество радиостанций

// Длины строк для конфигурации SPIFFS (используются в classes.h)
#define MAX_SSID_LEN          32
#define MAX_PWD_LEN           64
#define MAX_STATION_NAME_LEN  32
#define MAX_STATION_URL_LEN   128

// ============================================================
//  ПИНЫ КНОПОК M5STACK-C PLUS
// ============================================================
#define BUTTON_HOLD_TIME_MS 600
#define BTN_A_PIN 39 // Левая
#define BTN_B_PIN 38 // Средняя
#define BTN_C_PIN 37 // Правая


// ============================================================
//  НАСТРОЙКИ ДЛЯ GYRO JOYSTICK (MPU6886)
// ============================================================
// Пороги чувствительности
#define GYRO_ROLL_THRESHOLD     150.0f   // Крен (ось Y) — град/с
#define GYRO_PITCH_THRESHOLD    120.0f   // Тангаж (ось X) — град/с
#define GYRO_SHAKE_THRESHOLD    2.2f     // Встряхивание — в g (без учёта гравитации)

// Фильтры
#define GYRO_ALPHA              0.3f     // Коэффициент низкочастотного фильтра (0..1)
#define GYRO_MEDIAN_WINDOW      5        // Размер окна медианного фильтра

// Временные параметры
#define GYRO_SAMPLE_INTERVAL_MS 20       // Период опроса датчиков (мс)
#define GYRO_DEBOUNCE_MS        400      // Защита от повторных срабатываний (мс)
#define TIME_GYROSCOPE_BOUNCE   200      // Окно накопления/анализа жеста (мс)

// Команды GyroJoystick (возвращаемые значения)
#define GYRO_CMD_NONE           0
#define GYRO_CMD_SHAKE          1        // Play / Stop
#define GYRO_CMD_ROLL_DOWN      2        // Volume -
#define GYRO_CMD_ROLL_UP        3        // Volume +
#define GYRO_CMD_PITCH_BWD      4        // Station -
#define GYRO_CMD_PITCH_FWD      5        // Station +

// ============================================================
//  НАСТРОЙКИ ВРЕМЕНИ (NTP) И СЕТИ
// ============================================================
#define NTP_TIMEZONE_OFFSET  3.0f         // Часовой пояс (Москва UTC+3)
#define NTP_SERVER1          "pool.ntp.org"
#define NTP_SERVER2          "time.nist.gov"
#define NTP_SERVER3          "ru.pool.ntp.org"

#define NTP_RETRY_INTERVAL   20           // Интервал повтора в секундах при неудаче
#define NTP_SUCCESS_INTERVAL 3600         // Интервал синхронизации в секундах при успехе

#define WIFI_RETRY_DELAY_MS  500          // Задержка между попытками подключения к Wi-Fi
#define WIFI_TIMEOUT_MS      15000        // Таймаут подключения к Wi-Fi (15 секунд)

#define NET_STABILIZATION_DELAY_MS   60   // Время на завершение фоновых задач сети перед новым сокетом
#define START_WIFI_PROTECTION_MS     4000 // Защита от стартового шума антенны при включении

// =========================================================================
//  УДВОЕННЫЕ РАЗМЕРЫ СТЕКА ДЛЯ ЗАДАЧ FREERTOS (БЕЗОПАСНОСТЬ СЕТИ И ПЛЕЕРА)
// =========================================================================
#define AUDIO_TASK_STACK_SIZE   32768  // Было: 16384 (С запасом под тяжелые MP3 буферы)
#define CONTROL_TASK_STACK_SIZE 16384  // Было: 8192  (Для стабильной отрисовки и логики)
#define NETWORK_TASK_STACK_SIZE 32768  // Было: 16384 (Чтобы асинхронный сервер свободно переваривал 22 Кб)
#define GYRO_TASK_STACK_SIZE    8192   // Было: 4096  (С запасом под высокоточные расчеты джойстика)


#define AUDIO_TASK_PRIORITY     5         // Самый высокий приоритет для плавного звука
#define CONTROL_TASK_PRIORITY   2
#define GYRO_TASK_PRIORITY      2         // Тот же уровень, что и кнопки (опрос параллельный)
#define NETWORK_TASK_PRIORITY   1

#define CONTROL_TASK_DELAY_MS   25
#define NETWORK_TASK_DELAY_MS   100

// ============================================================
// ГЕОМЕТРИЯ ДИСПЛЕЯ M5STACK (ILI9341)
// ============================================================
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// === ОСТАЛЬНЫЕ СИСТЕМНЫЕ НАСТРОЙКИ ===
#define DEFAULT_BRIGHTNESS      80
#define DEBUG_MODE              1         // 1 = включена отладка в Serial, 0 = выключена

// ============================================================
//  СПИСОК РАДИОСТАНЦИЙ (ТОЛЬКО ЧИСТЫЙ HTTP И MP3!)
// ============================================================
#define STATION_LIST { \
    { "Monte-Carlo",            "http://montecarlo.hostingradio.ru:80/montecarlo128.mp3" }, \
    { "KWPX",                   "http://pavo.prostreaming.net:8034/stream?hash=1631464002154.mp3" }, \
    { "50 60 Hits",             "https://26433.live.streamtheworld.com/977_OLDIES.mp3" }, \
    { "AmericaCountry",         "http://ais-sa2.cdnstream1.com/1976_128.mp3" }, \
    { "R_Shanson",              "https://chanson.hostingradio.ru:8041/chanson-romantic128.mp3" }, \
    { "Radio 7",                "http://emgregion.hostingradio.ru:8064/moscow.radio7.mp3" }, \
    { "SomaCountry",            "http://ice2.somafm.com/bootliquor-320-mp3" }, \
    { "Carolina Radio",         "https://happy.radiocaroline319.nl/listen/caroline_oldies/radio320.mp3" }, \
    { "Dorojnoe",               "http://emgregion.hostingradio.ru:8064/moscow.dorognoe.mp3" }, \
    { "Retro FM",               "http://retroserver.streamr.ru:8043/retro256.mp3" } \
}

#endif