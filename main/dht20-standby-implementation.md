# DHT20传感器和待机画面实现说明

本文档详细说明在Xiaozhi ESP32项目中集成DHT20传感器和实现待机画面的相关改动。

## 功能概述

- **DHT20传感器集成**：通过I2C总线读取温度和湿度数据
- **待机画面实现**：在设备空闲时显示日期、星期、时间和温湿度信息
- **校准功能**：支持温度和湿度的校准偏移量调整
- **JSON数据输出**：支持以JSON格式输出传感器数据

## 硬件连接

| 信号 | GPIO引脚 | 传感器引脚 |
|------|---------|------------|
| SCL  | GPIO8   | SCL        |
| SDA  | GPIO9   | SDA        |
| VCC  | 3.3V    | VCC        |
| GND  | GND     | GND        |

## 代码结构

### 新增文件

1. **DHT20传感器驱动**
   - `main/dht20/dht20.h`：DHT20传感器驱动头文件
   - `main/dht20/dht20.cc`：DHT20传感器驱动实现

2. **传感器管理器**
   - `main/sensors/sensor_manager.h`：传感器管理类头文件
   - `main/sensors/sensor_manager.cc`：传感器管理类实现

3. **说明文档**
   - `main/dht20-standby-implementation.md`：本文档

### 修改文件

1. **应用程序初始化**
   - `main/application.cc`：添加I2C总线初始化和传感器管理器初始化

2. **显示系统**
   - `main/display/oled_display.cc`：实现待机画面的显示和更新逻辑

3. **构建配置**
   - `main/CMakeLists.txt`：添加新文件的源路径和包含路径

## 核心功能实现

### 1. DHT20传感器驱动

#### 主要功能
- 传感器初始化和复位
- 温度和湿度数据读取
- 校准偏移量设置
- JSON格式数据输出

#### 关键方法
- `Initialize()`：初始化传感器
- `ReadData(float& temperature, float& humidity)`：读取温度和湿度数据
- `SetTemperatureOffset(float offset)`：设置温度校准偏移量
- `SetHumidityOffset(float offset)`：设置湿度校准偏移量
- `GetJsonData()`：获取JSON格式的传感器数据

### 2. 传感器管理器

#### 主要功能
- 统一管理传感器的初始化
- 提供简洁的接口访问传感器数据
- 线程安全的数据读取

#### 关键方法
- `Initialize(i2c_master_bus_handle_t i2c_bus)`：初始化传感器管理器
- `ReadTemperatureHumidity(float& temperature, float& humidity)`：读取温湿度数据
- `GetTemperatureHumidityString()`：获取格式化的温湿度字符串
- `GetJsonData()`：获取JSON格式的传感器数据

### 3. 待机画面

#### 主要功能
- 在设备空闲时显示待机画面
- 显示日期、星期、时间（最大字体居中）、温湿度信息
- 待机画面优先级最低，不影响其他界面

#### 关键方法
- `SetupStandbyScreen()`：设置待机画面UI元素
- `ShowStandbyScreen()`：显示待机画面
- `HideStandbyScreen()`：隐藏待机画面
- `UpdateStandbyScreen()`：更新待机画面显示的信息

## 系统集成

### 应用程序初始化流程

1. 初始化I2C总线
2. 初始化传感器管理器
3. 在设备状态变化时管理待机画面的显示和隐藏
4. 定期更新待机画面的信息

### 设备状态与待机画面

| 设备状态 | 待机画面状态 |
|---------|------------|
| 空闲 (Idle) | 显示 |
| 连接中 (Connecting) | 隐藏 |
| 监听中 (Listening) | 隐藏 |
| 说话中 (Speaking) | 隐藏 |
| 激活中 (Activating) | 隐藏 |
| 升级中 (Upgrading) | 隐藏 |
| 配置中 (WifiConfiguring) | 隐藏 |

## 校准方法

### 温度校准

```cpp
// 通过传感器管理器设置温度校准偏移量
auto& sensor_manager = SensorManager::GetInstance();
sensor_manager.SetTemperatureOffset(0.5f); // 增加0.5°C
```

### 湿度校准

```cpp
// 通过传感器管理器设置湿度校准偏移量
auto& sensor_manager = SensorManager::GetInstance();
sensor_manager.SetHumidityOffset(-2.0f); // 减少2.0%
```

## 故障排查

### 传感器初始化失败

1. 检查硬件连接是否正确
2. 确认I2C总线初始化是否成功
3. 检查传感器是否正常供电

### 待机画面不显示

1. 确认设备是否处于空闲状态
2. 检查OLED显示是否正常工作
3. 查看日志中是否有相关错误信息

### 温湿度数据不准确

1. 使用校准功能调整偏移量
2. 确保传感器安装位置通风良好，避免阳光直射
3. 等待传感器稳定后再读取数据

## 技术细节

### I2C总线配置

- 使用ESP-IDF的I2C Master API
- 时钟频率：100kHz
- 启用内部上拉电阻

### 显示系统

- 使用LVGL库实现UI界面
- 待机画面使用四层布局：日期、星期、时间、温湿度
- 时间显示使用最大字体，居中显示

### 数据更新频率

- 待机画面信息每秒更新一次
- 传感器数据读取与待机画面更新同步

## 未来扩展

1. **支持更多传感器**：可在传感器管理器中添加其他类型的传感器
2. **远程校准**：通过网络接口实现远程校准功能
3. **数据记录**：添加温湿度数据记录功能，支持历史数据查看
4. **告警功能**：当温湿度超出设定范围时触发告警

## 参考资料

- [DHT20传感器 datasheet](https://www.aosong.com/product/dht20.html)
- [ESP-IDF I2C Master API文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html)
- [LVGL文档](https://docs.lvgl.io/)
