# 小智语音提醒TTS调试指南

## 当前问题分析

根据您的日志反馈，提醒功能流程基本正常，但存在以下问题：

### 1. 状态转换问题
```
W (83269) StateMachine: Invalid state transition: connecting -> speaking
```
**原因**：状态机不允许从connecting直接转换到speaking状态

### 2. TTS超时问题
```
W (93279) Application: Reminder TTS timeout, forcing completion
```
**原因**：服务器在30秒内没有返回TTS音频数据

## 已实施的修复

### 1. 状态机修复
- 修改状态转换顺序：connecting → listening → speaking
- 添加适当的状态转换延迟

### 2. 超时机制优化
- 将TTS超时时间从10秒延长到30秒
- 添加更详细的超时日志信息

### 3. 调试信息增强
- 添加完整的STT消息日志
- 增加服务器响应监控日志

## 测试验证步骤

### 1. 编译并烧录新固件
```bash
idf.py build
idf.py flash
idf.py monitor
```

### 2. 关键观察点

#### 2.1 状态转换日志
观察是否还出现状态转换错误：
```
期望看到：
I (timestamp) StateMachine: State: connecting -> listening
I (timestamp) StateMachine: State: listening -> speaking

而不是：
W (timestamp) StateMachine: Invalid state transition: connecting -> speaking
```

#### 2.2 STT消息发送
观察STT消息是否正确发送：
```
I (timestamp) Application: Full STT message to send: {"session_id":"xxx","type":"stt","text":"提醒时间到了，时间到了"}
I (timestamp) Application: STT message sent successfully to server
```

#### 2.3 TTS音频接收
重点关注是否有TTS start/stop消息：
```
期待看到：
I (timestamp) Application: TTS start received
I (timestamp) Application: TTS stop received
I (timestamp) Application: Reminder TTS finished, handling completion
```

### 3. 可能的问题排查

#### 3.1 服务器端问题
如果仍然超时，可能是：
- 服务器没有正确处理STT消息
- 服务器返回的TTS音频格式有问题
- 网络连接不稳定

#### 3.2 协议兼容性
检查使用的协议类型：
- MQTT协议 vs WebSocket协议
- 不同协议的STT消息格式可能略有差异

#### 3.3 音频处理链路
确认音频播放链路：
- OPUS解码是否正常
- I2S输出是否正常
- 音频codec初始化是否成功

## 预期改善效果

修复后应该能看到：
1. ✅ 状态转换无警告信息
2. ✅ STT消息成功发送到服务器
3. ✅ 服务器返回TTS音频数据
4. ✅ 设备正常播放TTS语音
5. ✅ TTS完成后正确回到idle状态

## 进一步调试建议

如果问题仍然存在，建议：
1. 检查服务器端日志，确认是否收到STT消息
2. 使用网络抓包工具监控MQTT/WebSocket通信
3. 在服务器端添加更详细的日志记录
4. 测试其他TTS功能是否正常工作

## 注意事项

- 确保设备已正确连接到网络
- 检查服务器地址和认证信息是否正确
- 验证设备时间和服务器时间是否同步
- 确认使用的协议版本兼容性