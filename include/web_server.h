#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Preferences.h>

// 定义常量
#define ANGLE_THRESHOLD 1.0f

// 历史记录结构体
struct FallHistoryItem {
    unsigned long time;  // 时间戳
    String type;        // 事件类型
    unsigned long duration; // 持续时间（秒）
};

class WebServerManager {
public:
    WebServerManager();
    void begin();
    void updateStatus(const String& status);
    void updateCommand(const String& command);
    
    // 新增方法
    void updateAngles(float pitch, float roll);
    void updateFallenState(bool fallen, unsigned long duration);
    void updateMonitoringState(bool active);
    void updateSystemInfo(const String& wifiStatus, unsigned long uptime, const String& lastCalibration);
    void addFallHistory(const String& type, unsigned long duration);
    void sendFallHistory();
    void update(float pitch, float roll, bool fallen, unsigned long fallDuration, bool monitoring);
    void setLastCalibrationTime(unsigned long time);
    void setMonitoringActive(bool active);
    
    // 回调函数类型定义
    using MonitoringCallback = void (*)(bool active);
    using CalibrateCallback = void (*)();
    using EmergencyCallback = void (*)();
    using VoiceCommandCallback = void (*)(const String& command);
    
    // 设置回调函数
    void setMonitoringCallback(MonitoringCallback callback);
    void setCalibrateCallback(CalibrateCallback callback);
    void setEmergencyCallback(EmergencyCallback callback);
    void setVoiceCommandCallback(VoiceCommandCallback callback);
    void processDNS();
    
    // 添加WiFi状态检查和重连功能
    void checkWiFiStatus();
    
private:
    AsyncWebServer server;
    AsyncWebSocket ws;
    Preferences preferences;
    
    // WiFi配置
    String WIFI_SSID;
    String WIFI_PASSWORD;
    bool isWiFiConfigured;
    
    // 常量定义
    static const int MAX_HISTORY_ITEMS = 10;  // 保存的历史记录最大数量
    // 使用宏替代静态成员变量
    static const int UPDATE_INTERVAL = 100;   // 更新间隔，单位毫秒
    
    // 私有变量
    float lastPitch;
    float lastRoll;
    unsigned long lastUpdateTime;
    unsigned long lastSystemInfoUpdate;
    unsigned long lastCalibrationTime;
    bool monitoringActive;
    
    // 历史记录数组
    FallHistoryItem fallHistory[MAX_HISTORY_ITEMS];
    int historyCount;
    
    // 回调函数
    MonitoringCallback monitoringCallback;
    CalibrateCallback calibrateCallback;
    EmergencyCallback emergencyCallback;
    VoiceCommandCallback voiceCommandCallback;
    
    // 私有方法
    void setupWiFi();  // 更改为setupWiFi
    void setupWebServer();
    bool shouldUpdateAngles(float pitch, float roll);
    void handleWebSocket(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                     AwsEventType type, void *arg, uint8_t *data, size_t len);
    void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
    void sendInitialData(AsyncWebSocketClient *client);
    
    // 添加WiFi重连相关变量和方法
    unsigned long lastWiFiCheckTime = 0;
    const unsigned long WIFI_CHECK_INTERVAL = 30000; // 30秒检查一次WiFi状态
    bool reconnectWiFi();
};

#endif