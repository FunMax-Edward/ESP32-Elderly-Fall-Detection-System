#ifndef ASR_COMMUNICATION_H
#define ASR_COMMUNICATION_H

#include <Arduino.h>

class AsrCommunication {
public:
    AsrCommunication();
    void begin(unsigned long baudRate = 19200);
    void update();
    void sendMessage(const String& message);
    void sendHeartbeat();
    
    // 回调函数类型定义
    using CommandCallback = void (*)(const String& command);
    void setCommandCallback(CommandCallback callback) { commandCallback = callback; }
    
    // 获取最后通信时间
    unsigned long getLastCommunicationTime() const { return lastCommunicationTime; }
    
    // 检查通信状态
    bool isConnected() const;
    String getStatusMessage() const;
    
private:
    static const unsigned long STATUS_INTERVAL = 5000;  // 状态报告间隔（毫秒）
    static const unsigned long CONNECTION_TIMEOUT = 10000;  // 连接超时时间（毫秒）
    
    unsigned long lastCommunicationTime;
    CommandCallback commandCallback;
};

#endif // ASR_COMMUNICATION_H