// ============================================================
//  GyroJoystick.h - Версия с полной поддержкой констант из config.h
// ============================================================

#ifndef GYRO_JOYSTICK_H
#define GYRO_JOYSTICK_H

#include <Arduino.h>
#include <M5Unified.h>
#include "config.h"   // Подключаем ваши макросы и настройки

class GyroJoystick {
public:
    GyroJoystick();
    ~GyroJoystick();

    bool begin();
    int update();  // Возвращает GYRO_CMD_* (0-5) из config.h

    // Геттеры для отладки
    float getSpeedRoll() const  { return _filteredGyroY; }
    float getSpeedPitch() const { return _filteredGyroX; }
    float getAccelZ() const     { return _filteredAccZ; }

private:
    // Сглаженные физические величины
    float _filteredAccX, _filteredAccY, _filteredAccZ;
    float _filteredGyroX, _filteredGyroY, _filteredGyroZ;
    float _gyroOffsetX, _gyroOffsetY, _gyroOffsetZ;

    // Массивы истории медианного фильтра (размер задан в config.h)
    float _gyroX_history[GYRO_MEDIAN_WINDOW];
    float _gyroY_history[GYRO_MEDIAN_WINDOW];
    int _histIndexX, _histIndexY;

    // Переменные вашего алгоритма окна накопления
    int _cmdFirst;                  // Текущее мгновенное состояние датчиков
    int _cmdNext;                   // Буферная команда внутри окна анализа
    unsigned long _analysisStart;   // Время фиксации первого триггера движения
    bool _analysisActive;           // Идет ли сейчас окно анализа 200 мс
    unsigned long _lastCommandTime; // Защита от дребезга (блокировка после выдачи команды)

    // Приватные методы калибровки и фильтрации
    void _calibrateGyro();
    float _lowPassFilter(float raw, float filtered);
    float _medianFilterX(float newValue);
    float _medianFilterY(float newValue);
    void _processSensorData(float ax, float ay, float az, float gx, float gy, float gz);
    
    // Внутренний мгновенный сканер физики (без задержек возвращает 0-5)
    int _checkPhysicalStatus(); 
};

#endif
