#include <Wire.h>
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#include "fall_detection.h"
#include "asr_communication.h"
#include "web_server.h"
#include "arduino_fs.h"

//http://192.168.4.1/


// 全局变量声明
FallDetection fallDetector;
AsrCommunication asrComm;
MPU6050 mpu;
WebServerManager webServer;

// 系统状态变量
unsigned long startTime;
unsigned long lastCalibrationTime = 0;

// 串口初始化函数
void serial_init() {
  // 使用默认USB CDC串口配置，不需要指定引脚
  Serial.begin(115200);
  // 等待串口准备就绪
  while (!Serial) {
    delay(10);
  }
  
  // ASR Pro串口配置保持不变
  Serial2.begin(19200, SERIAL_8N1, 16, 17);
  
  delay(1000);  // 给一些时间让串口稳定
  Serial.println("串口和ASR Pro通信初始化完成");
}

// 处理跌倒检测回调
void onFallDetected(const char* message) {
  Serial.print("⚠️ 警告：");
  Serial.println(message);
  asrComm.sendMessage(message);
  webServer.updateStatus(message);
  Serial.println("已向ASR Pro发送警告");
}

// 处理ASR Pro命令回调
void onAsrCommand(const String& command) {
  Serial.print("收到ASR命令: ");
  Serial.println(command);
  webServer.updateCommand(command);
  
  if (command.indexOf("open") >= 0) {
    Serial.println("尝试开启跌倒监测...");
    fallDetector.startMonitoring();
    webServer.setMonitoringActive(true);
    Serial.println("✓ 已开启跌倒监测，当前状态: " + String(fallDetector.isMonitoringActive() ? "开启" : "关闭"));
    asrComm.sendMessage("ESP32: 跌倒监测已开启");
  } 
  else if (command.indexOf("close") >= 0) {
    Serial.println("尝试关闭跌倒监测...");
    fallDetector.stopMonitoring();
    webServer.setMonitoringActive(false);
    Serial.println("✓ 已关闭跌倒监测，当前状态: " + String(fallDetector.isMonitoringActive() ? "开启" : "关闭"));
    asrComm.sendMessage("ESP32: 跌倒监测已关闭");
  }
}

// 网页监测状态改变回调
void onMonitoringChanged(bool active) {
  if (active) {
    fallDetector.startMonitoring();
    Serial.println("网页控制：开启跌倒监测");
    asrComm.sendMessage("ESP32: 跌倒监测已开启");
  } else {
    fallDetector.stopMonitoring();
    Serial.println("网页控制：关闭跌倒监测");
    asrComm.sendMessage("ESP32: 跌倒监测已关闭");
  }
}

// 网页校准传感器回调
void onCalibrate() {
  Serial.println("网页控制：开始校准传感器...");
  fallDetector.calibrate();
  lastCalibrationTime = millis();
  webServer.setLastCalibrationTime(lastCalibrationTime);
  Serial.println("校准完成");
  asrComm.sendMessage("ESP32: 传感器已校准");
}

// 网页紧急呼叫回调
void onEmergency() {
  Serial.println("⚠️ 紧急呼叫触发！");
  asrComm.sendMessage("紧急情况！请立即查看老人状态！");
  webServer.addFallHistory("紧急呼叫", 0);
}

// 网页语音命令回调
void onVoiceCommand(const String& command) {
  Serial.println("网页发送语音命令: " + command);
  onAsrCommand(command);
}

void setup() {
    serial_init();
    
    startTime = millis();
    
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    Wire.setPins(10, 11);             // 设置 SDA=10, SCL=11
    Wire.begin();
    #endif

    // 初始化文件系统并加载网页文件
    if (!initFS()) {
        Serial.println("文件系统初始化失败，使用内置网页");
    } else {
        Serial.println("文件系统初始化成功，已加载网页文件");
    }

    Serial.println(F("正在初始化传感器..."));
    if (!fallDetector.begin()) {
      Serial.println("MPU6050 初始化失败");
      while(true);
    }
    Serial.println("MPU6050 初始化成功");

    // 设置回调函数
    fallDetector.setFallDetectionCallback(onFallDetected);
    asrComm.setCommandCallback(onAsrCommand);
    asrComm.begin();
    
    // 初始化Web服务器
    webServer.begin();
    
    // 设置Web界面回调函数
    webServer.setMonitoringCallback(onMonitoringChanged);
    webServer.setCalibrateCallback(onCalibrate);
    webServer.setEmergencyCallback(onEmergency);
    webServer.setVoiceCommandCallback(onVoiceCommand);
    
    // 默认开启跌倒监测
    fallDetector.startMonitoring();
    webServer.setMonitoringActive(true);
    
    Serial.println("系统初始化完成，开始运行...");
}

void loop() {
  static unsigned long lastLoopTime = 0;
  unsigned long currentTime = millis();
  
  // 处理DNS请求 - 对网页访问至关重要
  webServer.processDNS();
  
  // 检查WiFi状态并在需要时重连
  webServer.checkWiFiStatus();
  
  // 处理来自电脑的串口数据
  if (Serial.available()) {
    String readUART = Serial.readString();
    readUART.trim();
    
    if (readUART == "test") {
      Serial.println("发送测试信号到ASR Pro...");
      asrComm.sendMessage("ESP32测试信号");
      Serial.println("测试信号已发送");
    } 
    else if (readUART == "c") {
      Serial.println("开始校准传感器...");
      fallDetector.calibrate();
      lastCalibrationTime = millis();
      webServer.setLastCalibrationTime(lastCalibrationTime);
      Serial.println("校准完成");
    } 
    else if (readUART == "r") {
      fallDetector.readSensorData();
    } 
    else if (readUART == "s") {
      fallDetector.startMonitoring();
      webServer.setMonitoringActive(true);
      Serial.println("开始跌倒监测...");
    } 
    else if (readUART == "p") {
      fallDetector.stopMonitoring();
      webServer.setMonitoringActive(false);
      Serial.println("暂停跌倒监测...");
    } 
    else {
      asrComm.sendMessage(readUART);
      Serial.println("已发送到ASR Pro: " + readUART);
    }
    
    readUART = "";
  }

  // 更新ASR通信状态
  asrComm.update();
  
  // 更新跌倒检测状态
  fallDetector.update();
  
  // 获取当前传感器数据
  Quaternion q;
  VectorFloat gravity;
  float ypr[3];
  
  if (fallDetector.getCurrentData(q, gravity, ypr)) {
    float pitch = ypr[1] * 180/M_PI;
    float roll = ypr[2] * 180/M_PI;
    bool fallenState = fallDetector.isFallenState();
    unsigned long fallDuration = fallDetector.getFallDuration();
    bool monitoringActive = fallDetector.isMonitoringActive();
    
    // 更新Web界面数据
    webServer.update(pitch, roll, fallenState, fallDuration, monitoringActive);
  }
  
  // 动态调整延迟时间
  unsigned long loopDuration = millis() - currentTime;
  if (loopDuration < 10) {  // 如果循环执行时间小于10ms
    delay(10 - loopDuration);  // 只延迟剩余时间
  }
}

/*操作命令：

- 'c': 校准传感器
- 'r': 读取当前角度数据
- 's': 开始跌倒监测
- 'p': 暂停跌倒监测
*/
