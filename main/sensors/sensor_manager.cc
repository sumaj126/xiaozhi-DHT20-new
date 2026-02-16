#include "sensor_manager.h"
#include "settings.h"
#include <esp_log.h>
#include <sstream>
#include <iomanip>

#define TAG "SensorManager"

SensorManager& SensorManager::GetInstance() {
    static SensorManager instance;
    return instance;
}

SensorManager::SensorManager() {
}

SensorManager::~SensorManager() {
}

bool SensorManager::Initialize(i2c_master_bus_handle_t i2c_bus) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        ESP_LOGI(TAG, "Sensor manager already initialized");
        return true;
    }

    try {
        // Initialize DHT20 sensor
        ESP_LOGI(TAG, "Creating DHT20 sensor instance");
        dht20_ = std::make_unique<DHT20>(i2c_bus);
        ESP_LOGI(TAG, "DHT20 sensor instance created successfully");
        
        if (!dht20_->Initialize()) {
            ESP_LOGE(TAG, "Failed to initialize DHT20 sensor");
            dht20_.reset();
            return false;
        }

        initialized_ = true;
        
        // Load calibration parameters
        LoadCalibration();
        
        ESP_LOGI(TAG, "Sensor manager initialized successfully");
        return true;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Exception during sensor initialization: %s", e.what());
        dht20_.reset();
        return false;
    } catch (...) {
        ESP_LOGE(TAG, "Unknown exception during sensor initialization");
        dht20_.reset();
        return false;
    }
}

bool SensorManager::ReadTemperatureHumidity(float& temperature, float& humidity) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !dht20_) {
        ESP_LOGE(TAG, "Sensor manager not initialized");
        return false;
    }

    bool result = dht20_->ReadData(temperature, humidity);
    if (result) {
        ESP_LOGI(TAG, "Successfully read temperature: %.2f°C, humidity: %.2f%%", temperature, humidity);
    }
    return result;
}

std::string SensorManager::GetTemperatureHumidityString() {
    float temperature, humidity;
    if (ReadTemperatureHumidity(temperature, humidity)) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1);
        ss << temperature << "°C / " << humidity << "%";
        return ss.str();
    }
    return "--.-°C / --.-%";
}

std::string SensorManager::GetJsonData() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !dht20_) {
        ESP_LOGE(TAG, "Sensor manager not initialized");
        return "{\"error\": \"Sensor manager not initialized\"}";
    }

    return dht20_->GetJsonData();
}

void SensorManager::SetTemperatureOffset(float offset) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (dht20_) {
        dht20_->SetTemperatureOffset(offset);
        SaveCalibration();
    }
}

void SensorManager::SetHumidityOffset(float offset) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (dht20_) {
        dht20_->SetHumidityOffset(offset);
        SaveCalibration();
    }
}

float SensorManager::GetTemperatureOffset() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (dht20_) {
        return dht20_->GetTemperatureOffset();
    }
    return 0.0f;
}

float SensorManager::GetHumidityOffset() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (dht20_) {
        return dht20_->GetHumidityOffset();
    }
    return 0.0f;
}

void SensorManager::LoadCalibration() {
    Settings settings("calibration", true);
    std::string temp_offset_str = settings.GetString("temperature_offset", "0.0");
    std::string hum_offset_str = settings.GetString("humidity_offset", "0.0");
    
    float temp_offset = std::stof(temp_offset_str);
    float hum_offset = std::stof(hum_offset_str);
    
    if (dht20_) {
        dht20_->SetTemperatureOffset(temp_offset);
        dht20_->SetHumidityOffset(hum_offset);
    }
    ESP_LOGI(TAG, "Loaded calibration: temp=%.2f, hum=%.2f", temp_offset, hum_offset);
}

void SensorManager::SaveCalibration() {
    if (!dht20_) {
        return;
    }
    
    float temp_offset = dht20_->GetTemperatureOffset();
    float hum_offset = dht20_->GetHumidityOffset();
    
    Settings settings("calibration", true);
    settings.SetString("temperature_offset", std::to_string(temp_offset));
    settings.SetString("humidity_offset", std::to_string(hum_offset));
    ESP_LOGI(TAG, "Saved calibration: temp=%.2f, hum=%.2f", temp_offset, hum_offset);
}
