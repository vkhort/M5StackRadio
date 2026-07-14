#include "GyroJoystick.h"

GyroJoystick::GyroJoystick() {
    _cmdFirst = GYRO_CMD_NONE;
    _cmdNext = GYRO_CMD_NONE;
    _analysisStart = 0;
    _analysisActive = false;
    _lastCommandTime = 0;

    _histIndexX = 0;
    _histIndexY = 0;
    _filteredAccX = 0.0f; _filteredAccY = 0.0f; _filteredAccZ = 1.0f;
    _filteredGyroX = 0.0f; _filteredGyroY = 0.0f; _filteredGyroZ = 0.0f;
    
    memset(_gyroX_history, 0, sizeof(_gyroX_history));
    memset(_gyroY_history, 0, sizeof(_gyroY_history));
}

GyroJoystick::~GyroJoystick() {}

bool GyroJoystick::begin() {
    M5.Imu.init(); // Аппаратное пробуждение чипа MPU6886
    delay(200);
    _calibrateGyro();
    return true;
}

void GyroJoystick::_calibrateGyro() {
    float gx_sum = 0, gy_sum = 0, gz_sum = 0;
    int samples = 100;
    for (int i = 0; i < samples; i++) {
        float gx, gy, gz;
        M5.Imu.getGyroData(&gx, &gy, &gz);
        gx_sum += gx; 
        gy_sum += gy; 
        gz_sum += gz;
        delay(5);
    }
    _gyroOffsetX = gx_sum / samples;
    _gyroOffsetY = gy_sum / samples;
    _gyroOffsetZ = gz_sum / samples;
}

float GyroJoystick::_lowPassFilter(float raw, float filtered) {
    return GYRO_ALPHA * raw + (1.0f - GYRO_ALPHA) * filtered;
}

float GyroJoystick::_medianFilterX(float newValue) {
    _gyroX_history[_histIndexX] = newValue;
    _histIndexX = (_histIndexX + 1) % GYRO_MEDIAN_WINDOW;
    float sorted[GYRO_MEDIAN_WINDOW];
    memcpy(sorted, _gyroX_history, sizeof(sorted));
    
    for (int i = 0; i < GYRO_MEDIAN_WINDOW - 1; i++) {
        for (int j = i + 1; j < GYRO_MEDIAN_WINDOW; j++) {
            if (sorted[i] > sorted[j]) { float t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t; }
        }
    }
    return sorted[GYRO_MEDIAN_WINDOW / 2];
}

float GyroJoystick::_medianFilterY(float newValue) {
    _gyroY_history[_histIndexY] = newValue;
    _histIndexY = (_histIndexY + 1) % GYRO_MEDIAN_WINDOW;
    float sorted[GYRO_MEDIAN_WINDOW];
    memcpy(sorted, _gyroY_history, sizeof(sorted));
    
    for (int i = 0; i < GYRO_MEDIAN_WINDOW - 1; i++) {
        for (int j = i + 1; j < GYRO_MEDIAN_WINDOW; j++) {
            if (sorted[i] > sorted[j]) { float t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t; }
        }
    }
    return sorted[GYRO_MEDIAN_WINDOW / 2];
}

void GyroJoystick::_processSensorData(float ax, float ay, float az, float gx, float gy, float gz) {
    gx -= _gyroOffsetX; gy -= _gyroOffsetY; gz -= _gyroOffsetZ;

    _filteredAccX = _lowPassFilter(ax, _filteredAccX);
    _filteredAccY = _lowPassFilter(ay, _filteredAccY);
    _filteredAccZ = _lowPassFilter(az, _filteredAccZ);

    _filteredGyroX = _lowPassFilter(gx, _filteredGyroX);
    _filteredGyroY = _lowPassFilter(gy, _filteredGyroY);
    _filteredGyroZ = _lowPassFilter(gz, _filteredGyroZ);
}

// Мгновенная проверка физического статуса на основе порогов из config.h
int GyroJoystick::_checkPhysicalStatus() {
    // 1. Проверяем встряхивание по оси Z (Высший приоритет)
    float acc_delta_z = fabs(_filteredAccZ - 1.0f);
    if (acc_delta_z > GYRO_SHAKE_THRESHOLD) {
        return GYRO_CMD_SHAKE; 
    }

    // Прогоняем гироскоп через скользящую медиану
    float gyroX_med = _medianFilterX(_filteredGyroX);
    float gyroY_med = _medianFilterY(_filteredGyroY);

    // 2. ТАНГАЖ (Ось X) -> Смена станций
    if (fabs(gyroX_med) > GYRO_PITCH_THRESHOLD) {
        return (gyroX_med > GYRO_PITCH_THRESHOLD) ? GYRO_CMD_PITCH_BWD : GYRO_CMD_PITCH_FWD; 
    }
    
    // 3. КРЕН (Ось Y) -> Изменение громкости (С корректными знаками)
    if (fabs(gyroY_med) > GYRO_ROLL_THRESHOLD) {
        return (gyroY_med > GYRO_ROLL_THRESHOLD) ? GYRO_CMD_ROLL_UP : GYRO_CMD_ROLL_DOWN; 
    }

    return GYRO_CMD_NONE; 
}

// === РЕАЛИЗАЦИЯ ВАШЕГО АЛГОРИТМА НАКОПЛЕНИЯ ЖЕСТОВ ===
int GyroJoystick::update() {
    unsigned long now = millis();
    float ax, ay, az, gx, gy, gz;
    
    M5.Imu.getAccelData(&ax, &ay, &az);
    M5.Imu.getGyroData(&gx, &gy, &gz);
    _processSensorData(ax, ay, az, gx, gy, gz);

    // Записываем мгновенное состояние в _cmdFirst
    _cmdFirst = _checkPhysicalStatus();

    // Защита от эха и дребезга после выполнения прошлой команды
    if (now - _lastCommandTime < GYRO_DEBOUNCE_MS) {
        return GYRO_CMD_NONE;
    }

    // Если анализ еще не идет — ловим первый стартовый импульс жеста
    if (!_analysisActive) {
        if (_cmdFirst != GYRO_CMD_NONE) {
            _cmdNext = _cmdFirst;
            _analysisStart = now;
            _analysisActive = true;
        }
        return GYRO_CMD_NONE; 
    }

    // Окно анализа открыто и идет сбор данных (200 мс)
    if (_analysisActive) {
        if (now - _analysisStart < TIME_GYROSCOPE_BOUNCE) {
            
            // ПРАВИЛО 1: Началось с SHAKE (1) или Крена (2,3) -> игнорируем любые изменения в _cmdFirst
            if (_cmdNext == GYRO_CMD_SHAKE || _cmdNext == GYRO_CMD_ROLL_DOWN || _cmdNext == GYRO_CMD_ROLL_UP) {
                // Ничего не делаем, сохраняем _cmdNext
            }
            // ПРАВИЛО 2: Началось с Тангажа (4,5), но в процессе замаха прилетел SHAKE (1) -> побеждает SHAKE
            else if ((_cmdNext == GYRO_CMD_PITCH_FWD || _cmdNext == GYRO_CMD_PITCH_BWD) && _cmdFirst == GYRO_CMD_SHAKE) {
                _cmdNext = GYRO_CMD_SHAKE;
            }
        } 
        // Время анализа (200 мс) истекло -> выдаем вердикт наружу
        else {
            int finalCmd = _cmdNext;
            
            // Полный сброс буферов анализа для следующего жеста
            _analysisActive = false;
            _cmdNext = GYRO_CMD_NONE;
            
            if (finalCmd != GYRO_CMD_NONE) {
                // Если сработал SHAKE, добавляем дополнительные 500 мс слепоты датчика Z,
                // чтобы полностью погасить рефлекторный возврат руки
                _lastCommandTime = (finalCmd == GYRO_CMD_SHAKE) ? now + 500 : now; 
                return finalCmd; // Возвращаем код жеста импульсно в этот конкретный кадр
            }
        }
    }

    return GYRO_CMD_NONE;
}
