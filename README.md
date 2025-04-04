# ESP32老人跌倒监测系统


## 项目简介

ESP32老人跌倒监测系统是一个基于ESP32和MPU6050传感器的智能监护设备，专为老年人的安全监护设计。系统通过精确分析姿态数据，实时检测老人的跌倒状态，并通过现代化的网页界面展示实时数据、发送警报通知。

本系统采用了现代Web技术实现了美观易用的用户界面，支持离线使用，并可通过语音或网页进行远程控制，是智能家居和远程看护的理想解决方案。

## 核心功能

- **实时跌倒检测**：基于MPU6050传感器，通过姿态角度和变化速率的复合分析，准确识别跌倒事件
- **3D可视化监测**：直观展示设备的实时姿态，包括俯仰角和横滚角的动态图表显示
- **即时警报通知**：检测到跌倒后立即发出警报，支持网页提醒和语音提示
- **远程监控控制**：通过网页界面随时查看老人状态，远程开启/关闭监测功能
- **语音交互**：支持语音命令控制系统，增强使用便捷性
- **离线功能**：即使网络不稳定，系统也能继续工作
- **跌倒历史记录**：自动记录跌倒事件，便于后期分析和查看

## 硬件要求

### 核心组件
- ESP32开发板（推荐ESP32-WROOM-32或ESP32-WROVER）
- MPU6050六轴加速度计和陀螺仪模块
- ASR Pro语音模块（可选）
- 电源适配器（5V/2A建议）
- 面包板及连接线

### 接线说明

| MPU6050 | ESP32 |
|---------|-------|
| VCC     | 3.3V  |
| GND     | GND   |
| SCL     | GPIO11|
| SDA     | GPIO10|
| INT     | 未使用 |

| ASR Pro | ESP32 |
|---------|-------|
| VCC     | 5V    |
| GND     | GND   |
| TX      | GPIO16|
| RX      | GPIO17|

## 软件依赖

### Arduino库
- ESP32 Arduino Core
- AsyncTCP
- ESPAsyncWebServer
- ArduinoJson
- I2Cdev
- MPU6050_6Axis_MotionApps20

### 开发环境
- PlatformIO或Arduino IDE（推荐PlatformIO）
- Web浏览器（Chrome/Firefox/Safari/Edge）

## 安装与配置

1. **克隆项目并安装依赖**
   ```bash
   git clone https://github.com/yourname/esp32-fall-detection.git
   cd esp32-fall-detection
   # 如果使用PlatformIO
   pio lib install
   ```

2. **设置WiFi配置**
   - 修改`src/web_server.cpp`文件中的默认WiFi设置（约40行处）：
   ```cpp
   WIFI_SSID = "你的WiFi名称";
   WIFI_PASSWORD = "你的WiFi密码";
   ```
   - 或者保持默认设置，之后通过网页界面进行配置

3. **编译与上传**
   ```bash
   # 使用PlatformIO
   pio run -t upload
   
   # 使用Arduino IDE
   # 打开Arduino IDE，选择ESP32开发板，然后编译上传
   ```

4. **上传文件系统**
   系统需要将网页文件上传到ESP32的SPIFFS文件系统中：
   ```bash
   # 使用PlatformIO
   pio run -t uploadfs
   ```

## 使用指南

1. **首次启动**
   - 上传程序和文件系统后，ESP32将自动启动
   - 系统会尝试连接您配置的WiFi网络
   - 成功连接后，控制台会显示分配的IP地址

2. **访问网页界面**
   - 在浏览器中输入ESP32的IP地址
   - 例如：`http://192.168.1.100/`
   - 加载完成后即可看到监测界面

3. **系统控制**
   - **开始/暂停监测**：控制老人跌倒监测的开启和关闭
   - **校准传感器**：在设备放置平稳后点击校准，确保数据准确性
   - **紧急呼叫**：手动触发紧急呼叫，适用于紧急但未被自动检测的情况
   - **WiFi配置**：修改系统连接的WiFi网络

4. **命令行控制**
   通过串口控制台可以发送以下命令：
   - `c`: 校准传感器
   - `r`: 读取当前角度数据
   - `s`: 开始跌倒监测
   - `p`: 暂停跌倒监测
   - `test`: 发送测试信号

## 界面说明

### 主界面组成
- **3D设备可视化**：直观展示设备当前姿态
- **俯仰角/横滚角图表**：实时显示角度变化趋势
- **系统控制按钮**：便捷的系统控制功能
- **系统信息面板**：显示WiFi状态、运行时间等信息
- **历史记录表格**：查看历史跌倒事件详情

### 界面特点
- 响应式设计，支持各种设备尺寸
- 暗色主题，适合长时间监控使用
- 现代化UI，采用毛玻璃效果和渐变色
- 流畅的动画和过渡效果

## 技术实现

### 系统架构
- **主控逻辑**：由main.cpp协调各个模块工作
- **传感器处理**：fall_detection模块处理MPU6050数据和跌倒检测算法
- **网络通信**：web_server模块处理HTTP请求和WebSocket实时通信
- **文件系统**：arduino_fs模块管理SPIFFS文件系统和网页文件
- **语音交互**：asr_communication模块处理语音命令和提示

### 跌倒检测算法
系统使用了融合角度阈值和角速度的复合检测算法：
1. 实时监测设备姿态角度（俯仰角和横滚角）
2. 计算角度变化速率，识别异常快速变化
3. 当角度超过阈值（60°）且变化速率较高时，判定为跌倒
4. 加入了姿态恢复检测，避免误报和漏报

### 网页技术
- 前端使用Vue 3和Element Plus构建
- WebSocket实现实时数据通信
- Service Worker支持离线缓存和使用
- 响应式设计适配不同设备

## 进阶定制

### 调整检测参数
可以在`fall_detection.h`文件中调整以下参数来优化检测效果：
```cpp
static constexpr float FALL_ANGLE_THRESHOLD = 60.0;    // 跌倒角度阈值（度）
static constexpr float FALL_RATE_THRESHOLD = 45.0;     // 角度变化速率阈值（度/秒）
static constexpr float STABLE_ANGLE_THRESHOLD = 40.0;  // 稳定角度阈值（度）
```

### 添加新功能
系统设计为模块化架构，便于扩展新功能：
- 添加更多传感器（如心率、温度等）
- 集成云平台通知（如微信、邮件等）
- 增加智能家居联动（如自动开灯、呼叫电梯等）

## 常见问题

1. **无法连接WiFi**
   - 检查WiFi名称和密码是否正确
   - 确认ESP32在WiFi信号覆盖范围内
   - 尝试通过网页界面重新配置WiFi

2. **传感器数据异常**
   - 确保MPU6050连接正确
   - 执行传感器校准（点击"校准传感器"按钮）
   - 检查I2C连接线是否松动

3. **网页无法访问**
   - 确认ESP32已成功连接WiFi
   - 检查浏览器输入的IP地址是否正确
   - 重启ESP32后再次尝试

## 贡献指南

欢迎对本项目提出改进建议和贡献代码！

1. Fork本仓库
2. 创建您的特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交您的更改 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建Pull Request

## 许可证

本项目采用MIT许可证 - 详见 [LICENSE](LICENSE) 文件

## 致谢

- 感谢所有开源库的贡献者
- 特别感谢[Element Plus](https://element-plus.org/)提供的UI组件
- 灵感来源于[Sui官网](https://sui.io/)的设计风格

---

项目开发者：[Edward]  
联系方式：[Edward.Max.cn@Gmail.com]

