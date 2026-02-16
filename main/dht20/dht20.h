#ifndef DHT20_H
#define DHT20_H

#include "boards/common/i2c_device.h"
#include <cstdint>
#include <string>

class DHT20 : public I2cDevice {
public:
    DHT20(i2c_master_bus_handle_t i2c_bus);
    ~DHT20();

    bool Initialize();
    bool ReadData(float& temperature, float& humidity);
    std::string GetSensorInfo();
    std::string GetJsonData();
    void SetTemperatureOffset(float offset);
    void SetHumidityOffset(float offset);
    float GetTemperatureOffset() const { return temperature_offset_; }
    float GetHumidityOffset() const { return humidity_offset_; }

private:
    static constexpr uint8_t DHT20_ADDR = 0x38;
    static constexpr uint8_t DHT20_CMD_READ = 0xAC;
    static constexpr uint8_t DHT20_CMD_SOFT_RESET = 0xBA;

    bool initialized_ = false;
    float temperature_offset_ = 0.0f;
    float humidity_offset_ = 0.0f;

    bool Reset();
    bool ReadStatus();
    bool WaitForReady();
};

#endif // DHT20_H
