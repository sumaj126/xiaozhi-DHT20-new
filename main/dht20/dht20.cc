#include "dht20.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "DHT20"

DHT20::DHT20(i2c_master_bus_handle_t i2c_bus)
    : I2cDevice(i2c_bus, DHT20_ADDR), temperature_offset_(0.0f), humidity_offset_(0.0f) {
}

DHT20::~DHT20() {
}

bool DHT20::Initialize() {
    ESP_LOGI(TAG, "Initializing DHT20 sensor");
    
    if (!Reset()) {
        ESP_LOGE(TAG, "Failed to reset sensor");
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    if (!ReadStatus()) {
        ESP_LOGE(TAG, "Failed to read sensor status");
        return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "DHT20 sensor initialized successfully");
    return true;
}

bool DHT20::Reset() {
    uint8_t cmd = DHT20_CMD_SOFT_RESET;
    esp_err_t err = i2c_master_transmit(i2c_device_, &cmd, 1, 100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send reset command: %d", err);
        return false;
    }
    return true;
}

bool DHT20::ReadStatus() {
    uint8_t status;
    esp_err_t err = i2c_master_receive(i2c_device_, &status, 1, 100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read status: %d", err);
        return false;
    }
    ESP_LOGI(TAG, "DHT20 status: 0x%02X", status);
    return true;
}

bool DHT20::WaitForReady() {
    for (int i = 0; i < 10; i++) {
        uint8_t status;
        esp_err_t err = i2c_master_receive(i2c_device_, &status, 1, 100);
        if (err == ESP_OK && !(status & 0x80)) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGE(TAG, "Sensor not ready");
    return false;
}

bool DHT20::ReadData(float& temperature, float& humidity) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Sensor not initialized");
        return false;
    }

    // Send read command
    uint8_t cmd[] = {DHT20_CMD_READ, 0x33, 0x00};
    esp_err_t err = i2c_master_transmit(i2c_device_, cmd, 3, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send read command: %d", err);
        return false;
    }

    // Wait for measurement (80ms)
    vTaskDelay(pdMS_TO_TICKS(80));

    // Read data
    uint8_t data[7] = {0};
    err = i2c_master_receive(i2c_device_, data, 7, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read data: %d", err);
        return false;
    }

    // Check status bit (bit 7 of first byte should be 0 when ready)
    if (data[0] & 0x80) {
        ESP_LOGE(TAG, "Measurement not ready");
        return false;
    }

    // Parse data (same as reference project)
    uint32_t raw_humidity = ((data[1] << 12) | (data[2] << 4) | ((data[3] & 0xF0) >> 4));
    uint32_t raw_temperature = (((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5]);

    // Convert to actual values
    humidity = (raw_humidity * 100.0f) / 1048576.0f + humidity_offset_;
    temperature = (raw_temperature * 200.0f) / 1048576.0f - 50.0f + temperature_offset_;

    ESP_LOGI(TAG, "Temperature: %.2f°C, Humidity: %.2f%%", temperature, humidity);
    return true;
}

std::string DHT20::GetJsonData() {
    float temperature, humidity;
    if (ReadData(temperature, humidity)) {
        char json[100];
        snprintf(json, sizeof(json), 
                 "{\"temperature\": %.2f, \"humidity\": %.2f}", 
                 temperature, humidity);
        return std::string(json);
    }
    return "{\"error\": \"Failed to read DHT20\"}";
}

void DHT20::SetTemperatureOffset(float offset) {
    temperature_offset_ = offset;
    ESP_LOGI(TAG, "Temperature offset set to %.2f", offset);
}

void DHT20::SetHumidityOffset(float offset) {
    humidity_offset_ = offset;
    ESP_LOGI(TAG, "Humidity offset set to %.2f", offset);
}

std::string DHT20::GetSensorInfo() {
    return "DHT20 Temperature and Humidity Sensor";
}
