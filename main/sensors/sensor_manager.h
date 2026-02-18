#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "dht20/dht20.h"
#include <memory>
#include <mutex>

class SensorManager {
public:
    static SensorManager& GetInstance();

    bool Initialize(i2c_master_bus_handle_t i2c_bus);
    bool ReadTemperatureHumidity(float& temperature, float& humidity);
    std::string GetTemperatureHumidityString();
    std::string GetJsonData();
    void SetTemperatureOffset(float offset);
    void SetHumidityOffset(float offset);
    float GetTemperatureOffset() const;
    float GetHumidityOffset() const;
    void LoadCalibration();
    void SaveCalibration();

private:
    SensorManager();
    ~SensorManager();

    std::unique_ptr<DHT20> dht20_;
    mutable std::mutex mutex_;
    bool initialized_ = false;
};

#endif // SENSOR_MANAGER_H
