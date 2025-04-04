#include "web_server.h"
#include "arduino_fs.h"
#include <ArduinoJson.h>

// 移除全局变量，改为类成员变量
// const char* WIFI_SSID = "ESP32-老人监护";
// const char* WIFI_PASSWORD = "12345678";

// 从index.html读取的内容会被放在这里
// 使用extern关键字引用arduino_fs中存储的HTML文件
// extern const char index_html[] PROGMEM; // 不再需要，因为已经在arduino_fs.h中声明

WebServerManager::WebServerManager() : 
    server(80), 
    ws("/ws"),
    lastPitch(0),
    lastRoll(0),
    WIFI_SSID(""),  // 初始化为空，等待配置
    WIFI_PASSWORD(""),
    monitoringActive(false) {} 

void WebServerManager::begin() {
    // 从Preferences中读取WiFi配置
    preferences.begin("wifi", false);
    String ssid = preferences.getString("ssid", "");
    String password = preferences.getString("password", "");
    isWiFiConfigured = preferences.getBool("configured", false);
    preferences.end();
    
    // 如果有配置过WiFi，使用存储的配置
    if(isWiFiConfigured && ssid.length() > 0) {
        WIFI_SSID = ssid;
        WIFI_PASSWORD = password;
    } else {
        // 默认WiFi设置（如果用户未设置）
        WIFI_SSID = "清泉石上流";  // 请替换为您的WiFi名称
        WIFI_PASSWORD = "123456789";  // 请替换为您的WiFi密码
    }
    
    setupWiFi();  // 使用STA模式连接到WiFi
    
    // 初始化SPIFFS
    if(!SPIFFS.begin(true)) {
        Serial.println("警告: SPIFFS挂载失败，部分功能可能不可用");
    } else {
        Serial.println("SPIFFS挂载成功");
        // 列出所有文件
        File root = SPIFFS.open("/");
        File file = root.openNextFile();
        while(file) {
            Serial.print("发现文件: ");
            Serial.println(file.name());
            file = root.openNextFile();
        }
    }
    
    setupWebServer();
    lastUpdateTime = 0;
    lastSystemInfoUpdate = 0;
    // 初始化历史记录数组
    for (int i = 0; i < MAX_HISTORY_ITEMS; i++) {
        fallHistory[i].time = 0;
        fallHistory[i].type = "";
        fallHistory[i].duration = 0;
    }
    historyCount = 0;
}

void WebServerManager::setupWiFi() {
    WiFi.mode(WIFI_STA);  // 设置为Station模式
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
    
    Serial.println("正在连接到WiFi网络: " + WIFI_SSID);
    
    // 等待连接，最多等待20秒
    int connectTimeout = 20;  // 20秒超时
    while (WiFi.status() != WL_CONNECTED && connectTimeout > 0) {
        delay(1000);
        Serial.print(".");
        connectTimeout--;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi连接成功!");
        Serial.print("IP地址: ");
        Serial.println(WiFi.localIP());
        
        // 保存成功的配置
        preferences.begin("wifi", false);
        preferences.putString("ssid", WIFI_SSID);
        preferences.putString("password", WIFI_PASSWORD);
        preferences.putBool("configured", true);
        preferences.end();
    } else {
        Serial.println("\nWiFi连接失败! 请检查网络配置。");
    }
}

void WebServerManager::setupWebServer() {
    // 设置WebSocket事件处理
    ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, 
                     AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->handleWebSocket(server, client, type, arg, data, len);
    });
    
    // 添加WebSocket处理器
    server.addHandler(&ws);
    
    // 设置根路径处理 - 返回主HTML页面
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });
    
    // 添加对Service Worker文件的支持
    server.on("/sw.js", HTTP_GET, [](AsyncWebServerRequest *request){
        if(SPIFFS.exists("/sw.js")) {
            request->send(SPIFFS, "/sw.js", "application/javascript");
        } else {
            request->send(404, "text/plain", "Service Worker not found");
            Serial.println("警告: sw.js文件未找到");
        }
    });
    
    // 添加对CSS文件的支持
    server.on("/modern.css", HTTP_GET, [](AsyncWebServerRequest *request){
        if(SPIFFS.exists("/modern.css")) {
            request->send(SPIFFS, "/modern.css", "text/css");
        } else {
            request->send(404, "text/plain", "CSS file not found");
            Serial.println("警告: modern.css文件未找到");
        }
    });
    
    // 启动Web服务器
    server.begin();
    Serial.println("Web服务器已启动");
}

void WebServerManager::updateStatus(const String& status) {
    String json = "{\"type\":\"status\",\"value\":\"" + status + "\"}";
    ws.textAll(json);
}

void WebServerManager::updateCommand(const String& command) {
    String json = "{\"type\":\"command\",\"value\":\"" + command + "\"}";
    ws.textAll(json);
}

// 检查是否需要更新角度数据
bool WebServerManager::shouldUpdateAngles(float pitch, float roll) {
    float pitchDiff = abs(pitch - lastPitch);
    float rollDiff = abs(roll - lastRoll);
    
    if (pitchDiff > ANGLE_THRESHOLD || rollDiff > ANGLE_THRESHOLD) {
        lastPitch = pitch;
        lastRoll = roll;
        return true;
    }
    return false;
}

// 更新角度数据
void WebServerManager::updateAngles(float pitch, float roll) {
    if (shouldUpdateAngles(pitch, roll)) {
        char json[100];
        snprintf(json, sizeof(json), "{\"type\":\"angles\",\"pitch\":%.2f,\"roll\":%.2f}", pitch, roll);
        ws.textAll(json);
    }
}

// 添加新的方法来更新跌倒状态
void WebServerManager::updateFallenState(bool fallen, unsigned long duration) {
    char json[100];
    snprintf(json, sizeof(json), "{\"type\":\"fallen_state\",\"fallen\":%s,\"duration\":%lu}", 
             fallen ? "true" : "false", duration / 1000); // 转换为秒
    ws.textAll(json);
    
    // 如果是新的跌倒事件，添加到历史记录
    if (fallen && duration == 0) {
        addFallHistory("跌倒警报", duration / 1000);
    }
}

// 添加新的方法来更新监测状态
void WebServerManager::updateMonitoringState(bool active) {
    char json[100];
    snprintf(json, sizeof(json), "{\"type\":\"monitoring\",\"active\":%s}", 
             active ? "true" : "false");
    ws.textAll(json);
}

// 添加新的方法来更新系统信息
void WebServerManager::updateSystemInfo(const String& wifiStatus, unsigned long uptime, const String& lastCalibration) {
    String currentWifiStatus;
    
    if (WiFi.status() == WL_CONNECTED) {
        currentWifiStatus = "已连接到: " + WIFI_SSID + " (" + WiFi.localIP().toString() + ")";
    } else {
        currentWifiStatus = "未连接";
    }
    
    char json[200];
    snprintf(json, sizeof(json), 
             "{\"type\":\"system_info\",\"wifi_status\":\"%s\",\"uptime\":%lu,\"last_calibration\":\"%s\"}", 
             currentWifiStatus.c_str(), uptime / 1000, lastCalibration.c_str());
    ws.textAll(json);
}

// 添加历史记录
void WebServerManager::addFallHistory(const String& type, unsigned long duration) {
    // 移动所有记录，为新记录腾出空间
    for (int i = MAX_HISTORY_ITEMS - 1; i > 0; i--) {
        fallHistory[i] = fallHistory[i-1];
    }
    
    // 添加新记录
    fallHistory[0].time = millis();
    fallHistory[0].type = type;
    fallHistory[0].duration = duration;
    
    if (historyCount < MAX_HISTORY_ITEMS) {
        historyCount++;
    }
    
    // 发送更新的历史记录
    sendFallHistory();
}

// 发送历史记录到客户端
void WebServerManager::sendFallHistory() {
    if (historyCount == 0) return;
    
    String json = "{\"type\":\"fall_history\",\"events\":[";
    
    for (int i = 0; i < historyCount; i++) {
        if (i > 0) json += ",";
        
        json += "{\"time\":" + String(fallHistory[i].time);
        json += ",\"type\":\"" + fallHistory[i].type + "\"";
        json += ",\"duration\":" + String(fallHistory[i].duration) + "}";
    }
    
    json += "]}";
    ws.textAll(json);
}

// 定期更新所有数据
void WebServerManager::update(float pitch, float roll, bool fallen, unsigned long fallDuration, bool monitoring) {
    unsigned long currentTime = millis();
    
    // 限制更新频率
    if (currentTime - lastUpdateTime >= UPDATE_INTERVAL) {
        // 使用数据压缩更新角度
        updateAngles(pitch, roll);
        
        // 只在状态改变时更新状态
        static bool lastFallenState = false;
        if (lastFallenState != fallen) {
            updateFallenState(fallen, fallDuration);
            lastFallenState = fallen;
        }
        
        // 只在状态改变时更新监测状态
        static bool lastMonitoringState = false;
        if (lastMonitoringState != monitoring) {
            updateMonitoringState(monitoring);
            lastMonitoringState = monitoring;
        }
        
        // 每10秒更新一次系统信息
        if (currentTime - lastSystemInfoUpdate >= 10000) {
            String wifiStatus = "已连接";
            String lastCalibration = lastCalibrationTime == 0 ? "未校准" : String(lastCalibrationTime / 1000) + "秒前";
            updateSystemInfo(wifiStatus, millis(), lastCalibration);
            lastSystemInfoUpdate = currentTime;
        }
        
        lastUpdateTime = currentTime;
    }
}

void WebServerManager::setLastCalibrationTime(unsigned long time) {
    lastCalibrationTime = time;
}

void WebServerManager::handleWebSocket(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                     AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket客户端 #%u 已连接，IP: %s\n", client->id(), client->remoteIP().toString().c_str());
            // 连接时发送初始数据
            sendInitialData(client);
            break;
            
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket客户端 #%u 已断开\n", client->id());
            break;
            
        case WS_EVT_DATA:
            Serial.printf("WebSocket数据接收: 客户端 #%u, 数据长度: %d\n", client->id(), len);
            handleWebSocketMessage(arg, data, len);
            break;
            
        case WS_EVT_PONG:
            Serial.printf("WebSocket pong: 客户端 #%u\n", client->id());
            break;

        case WS_EVT_ERROR:
            Serial.printf("WebSocket错误: 客户端 #%u, %s\n", client->id(), (char*)data);
            break;
    }
}

void WebServerManager::sendInitialData(AsyncWebSocketClient *client) {
    // 发送当前状态
    String statusJson = "{\"type\":\"status\",\"value\":\"系统正常运行中\"}";
    client->text(statusJson);
    
    // 发送监测状态
    char monitoringJson[100];
    snprintf(monitoringJson, sizeof(monitoringJson), 
             "{\"type\":\"monitoring\",\"active\":%s}", 
             monitoringActive ? "true" : "false");
    client->text(monitoringJson);
    
    // 发送历史记录
    if (historyCount > 0) {
        String historyJson = "{\"type\":\"fall_history\",\"events\":[";
        
        for (int i = 0; i < historyCount; i++) {
            if (i > 0) historyJson += ",";
            
            historyJson += "{\"time\":" + String(fallHistory[i].time);
            historyJson += ",\"type\":\"" + fallHistory[i].type + "\"";
            historyJson += ",\"duration\":" + String(fallHistory[i].duration) + "}";
        }
        
        historyJson += "]}";
        client->text(historyJson);
    }
}

void WebServerManager::handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;

        // 使用 ArduinoJson 解析消息
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (char*)data, DeserializationOption::NestingLimit(10));

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            return; // 解析失败，不做处理
        }

        // 检查是否存在 "action" 字段且其值为字符串
        if (doc["action"].is<const char*>()) { // 修改：使用 is<T>() 替代 containsKey
            const char* action = doc["action"]; // 获取 action 的值

            if (strcmp(action, "start_monitoring") == 0) {
                Serial.println("收到指令: start_monitoring"); // 增加调试信息
                // monitoringActive = true; // 状态应由主逻辑控制并通过 update 传入
                if (monitoringCallback) {
                    monitoringCallback(true); // 通知主逻辑开启监测
                }
                // updateMonitoringState(true); // 状态更新应基于主逻辑的回调或 update 函数的参数
            }
            else if (strcmp(action, "stop_monitoring") == 0) {
                Serial.println("收到指令: stop_monitoring"); // 增加调试信息
                // monitoringActive = false; // 状态应由主逻辑控制并通过 update 传入
                if (monitoringCallback) {
                    monitoringCallback(false); // 通知主逻辑关闭监测
                }
                // updateMonitoringState(false); // 状态更新应基于主逻辑的回调或 update 函数的参数
            }
            else if (strcmp(action, "calibrate") == 0) {
                Serial.println("收到指令: calibrate");
                if (calibrateCallback) {
                    calibrateCallback();
                }
                lastCalibrationTime = millis();
                 // 可以考虑发送校准确认消息给客户端
                 // updateStatus("校准命令已接收");
            }
            else if (strcmp(action, "emergency") == 0) {
                Serial.println("收到指令: emergency");
                if (emergencyCallback) {
                    emergencyCallback();
                }
                updateStatus("紧急呼叫已触发！"); // 这个可以立即更新状态
            }
            else if (strcmp(action, "voice_open") == 0) {
                Serial.println("收到指令: voice_open");
                if (voiceCommandCallback) {
                    voiceCommandCallback("open");
                }
            }
            else if (strcmp(action, "voice_close") == 0) {
                Serial.println("收到指令: voice_close");
                if (voiceCommandCallback) {
                    voiceCommandCallback("close");
                }
            }
            else if (strcmp(action, "set_wifi_config") == 0) {
                Serial.println("收到指令: set_wifi_config");
                
                // 获取WiFi配置
                if (doc["ssid"].is<String>() && doc["password"].is<String>()) {
                    String newSSID = doc["ssid"].as<String>();
                    String newPassword = doc["password"].as<String>();
                    
                    Serial.print("新的WiFi配置 - SSID: ");
                    Serial.print(newSSID);
                    Serial.println(", 密码: ********");
                    
                    // 保存WiFi配置到Preferences
                    preferences.begin("wifi", false);
                    preferences.putString("ssid", newSSID);
                    preferences.putString("password", newPassword);
                    preferences.putBool("configured", true);
                    preferences.end();
                    
                    // 更新当前配置
                    WIFI_SSID = newSSID;
                    WIFI_PASSWORD = newPassword;
                    
                    // 发送确认消息
                    String json = "{\"type\":\"wifi_config_result\",\"success\":true,\"message\":\"WiFi配置已保存，设备将重新连接\"}";
                    ws.textAll(json);
                    
                    // 尝试重新连接WiFi
                    if (reconnectWiFi()) {
                        String resultJson = "{\"type\":\"wifi_config_result\",\"success\":true,\"message\":\"已成功连接到 " + newSSID + "\"}";
                        ws.textAll(resultJson);
                    } else {
                        String resultJson = "{\"type\":\"wifi_config_result\",\"success\":false,\"message\":\"无法连接到 " + newSSID + "，请检查配置\"}";
                        ws.textAll(resultJson);
                    }
                } else {
                    String json = "{\"type\":\"wifi_config_result\",\"success\":false,\"message\":\"WiFi配置不完整\"}";
                    ws.textAll(json);
                }
            }
            else {
                 Serial.print("收到未知指令: ");
                 Serial.println(action);
            }
        } else {
            Serial.println("收到的消息缺少 'action' 字段或其值不是字符串"); // 修改错误信息
            // 打印收到的原始消息以供调试
            Serial.print("原始消息: ");
            Serial.println((char*)data);
        }
    }
}

void WebServerManager::setMonitoringCallback(MonitoringCallback callback) {
    monitoringCallback = callback;
}

void WebServerManager::setCalibrateCallback(CalibrateCallback callback) {
    calibrateCallback = callback;
}

void WebServerManager::setEmergencyCallback(EmergencyCallback callback) {
    emergencyCallback = callback;
}

void WebServerManager::setVoiceCommandCallback(VoiceCommandCallback callback) {
    voiceCommandCallback = callback;
}

void WebServerManager::setMonitoringActive(bool active) {
    monitoringActive = active;
}

// 更新处理DNS请求的方法 - STA模式下不需要DNS服务器
void WebServerManager::processDNS() {
    // STA模式下不需要处理DNS请求
    // 如果需要mDNS服务可以在这里添加
}

// 实现WiFi状态检查功能
void WebServerManager::checkWiFiStatus() {
    unsigned long currentTime = millis();
    
    // 每30秒检查一次WiFi状态
    if (currentTime - lastWiFiCheckTime >= WIFI_CHECK_INTERVAL) {
        lastWiFiCheckTime = currentTime;
        
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi连接已断开，尝试重连...");
            reconnectWiFi();
        }
    }
}

// 实现WiFi重连功能
bool WebServerManager::reconnectWiFi() {
    // 如果WiFi已经连接，不需要重连
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }
    
    Serial.print("尝试重新连接到WiFi: ");
    Serial.println(WIFI_SSID);
    
    // 断开当前连接
    WiFi.disconnect();
    delay(1000);
    
    // 开始新的连接
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
    
    // 等待连接，最多等待10秒
    int connectTimeout = 10;
    while (WiFi.status() != WL_CONNECTED && connectTimeout > 0) {
        delay(1000);
        Serial.print(".");
        connectTimeout--;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi重连成功!");
        Serial.print("IP地址: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        Serial.println("\nWiFi重连失败!");
        return false;
    }
}