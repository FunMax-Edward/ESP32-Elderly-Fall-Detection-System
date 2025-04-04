#include "asr_communication.h"

AsrCommunication::AsrCommunication() : 
    lastCommunicationTime(0),
    commandCallback(nullptr) {
}

void AsrCommunication::begin(unsigned long baudRate) {
    Serial2.begin(baudRate);
}

// 在update方法中确保正确处理接收到的数据
void AsrCommunication::update() {
  if (Serial2.available()) {
    String receivedString = Serial2.readString();
    receivedString.trim();
    
    // 添加调试输出
    Serial.print("ASR原始数据: [");
    Serial.print(receivedString);
    Serial.println("]");
    
    // 更新最后通信时间 - 修复变量名
    lastCommunicationTime = millis();
    
    // 调用回调函数处理命令
    if (commandCallback && receivedString.length() > 0) {
      commandCallback(receivedString);
    }
  }
    
    // 发送心跳包
    if (millis() - lastCommunicationTime > STATUS_INTERVAL) {
        sendHeartbeat();
    }
}

void AsrCommunication::sendMessage(const String& message) {
    Serial2.println(message);
    lastCommunicationTime = millis();
}

void AsrCommunication::sendHeartbeat() {
    sendMessage("ESP32: 心跳包");
}

bool AsrCommunication::isConnected() const {
    return (millis() - lastCommunicationTime) < CONNECTION_TIMEOUT;
}

String AsrCommunication::getStatusMessage() const {
    if (lastCommunicationTime == 0) {
        return "尚未建立连接";
    }
    
    String status = "上次通信在 ";
    status += String((millis() - lastCommunicationTime) / 1000.0);
    status += " 秒前";
    return status;
}