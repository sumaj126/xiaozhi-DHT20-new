# 小智语音提醒TTS修复测试指南

## 问题描述
原项目中提醒时间功能可以正常触发，服务器也能响应，但无法实现TTS语音播报。

## 修复内容

### 1. 核心问题修复
- 完善了提醒TTS的状态管理机制
- 添加了TTS播放确认和超时处理
- 改进了错误处理和资源清理

### 2. 新增功能
- 添加了TTS超时保护机制（10秒超时）
- 增强了状态转换的安全性
- 完善了日志记录便于调试

### 3. 修改的文件
- `main/application.cc` - 核心提醒TTS逻辑
- `main/application.h` - 新增辅助方法声明

## 测试步骤

### 1. 编译固件
```bash
idf.py build
```

### 2. 烧录固件
```bash
idf.py flash
```

### 3. 监控串口日志
```bash
idf.py monitor
```

### 4. 功能测试

#### 测试场景1：相对时间提醒
1. 对设备说："5分钟后提醒我开会"
2. 观察日志输出：
   ```
   I (timestamp) Application: Parsing reminder command: 5分钟后提醒我开会
   I (timestamp) Application: Parsed relative reminder: 5 minutes, message: 开会
   I (timestamp) Application: Setting reminder for 300 seconds
   I (timestamp) Application: Showing reminder confirmation: 已设置5分0秒后提醒：开会
   ```

#### 测试场景2：绝对时间提醒
1. 对设备说："下午3点提醒我喝水"
2. 观察日志输出相应的解析结果

#### 测试场景3：提醒触发测试
1. 设置一个短时间提醒（如1分钟）
2. 等待提醒触发
3. 观察以下关键日志：
   ```
   I (timestamp) Application: === REMINDER CALLBACK INVOKED ===
   I (timestamp) Application: Reminder triggered! Message: 开会
   I (timestamp) Application: Requesting server TTS for reminder
   I (timestamp) Application: Sending STT message: 提醒时间到了，开会
   I (timestamp) Application: STT message sent successfully
   ```

#### 测试场景4：TTS播放确认
1. 观察设备是否播放TTS语音
2. 查看服务器是否返回TTS音频数据
3. 确认设备状态正确转换：
   - 提醒触发时：`kDeviceStateSpeaking`
   - TTS完成后：`kDeviceStateIdle`

## 预期行为

### 成功情况
1. 提醒触发时设备播放提示音
2. 服务器接收STT消息并返回TTS音频
3. 设备播放服务器返回的TTS语音
4. 播放完成后自动回到空闲状态
5. 日志显示完整的处理流程

### 失败情况处理
1. 如果TTS发送失败：显示错误提示，回到空闲状态
2. 如果TTS超时（10秒）：强制完成，显示超时提示
3. 如果服务器无响应：显示错误信息，清理资源

## 调试要点

### 关键日志标识
- `[TAG] "=== REMINDER CALLBACK INVOKED ==="` - 提醒回调触发
- `[TAG] "Requesting server TTS for reminder"` - 请求服务器TTS
- `[TAG] "STT message sent successfully"` - STT消息发送成功
- `[TAG] "Reminder TTS finished, handling completion"` - TTS完成处理
- `[TAG] "Handling reminder failure"` - 提醒失败处理

### 常见问题排查
1. **提醒不触发**：检查`reminder_timer_`是否正确初始化
2. **TTS不播放**：确认网络连接和服务器配置
3. **状态卡住**：查看是否有未清理的定时器或资源
4. **重复提醒**：检查`reminder_tts_active_`标志位管理

## 注意事项

1. 确保设备已连接到网络且能访问TTS服务器
2. 测试时建议使用较短的时间间隔（1-2分钟）
3. 观察串口日志确认每个步骤的执行情况
4. 如遇问题可根据日志定位具体环节的故障点

## 版本信息
- 修复版本：基于最新代码
- 适用固件：支持提醒功能的所有版本
- 兼容性：向前兼容现有提醒设置功能