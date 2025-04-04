#ifndef FALL_DETECTION_H
#define FALL_DETECTION_H

#include <Wire.h>
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"

class FallDetection {
public:
    FallDetection();
    bool begin();
    void calibrate();
    void readSensorData();
    void startMonitoring();
    void stopMonitoring();
    void update();
    bool isMonitoringActive() const { return isMonitoring; }
    
    bool isFallenState() const { return fallenState; }
    unsigned long getFallDuration() const { 
        return fallenState ? (millis() - fallStartTime) : 0; 
    }
    bool getCurrentData(Quaternion &q, VectorFloat &gravity, float (&ypr)[3]);
    
    // 回调函数类型定义
    using FallDetectionCallback = void (*)(const char* message);
    void setFallDetectionCallback(FallDetectionCallback callback) { fallCallback = callback; }

private:
    MPU6050 mpu;
    bool DMPReady;
    uint8_t devStatus;
    uint16_t packetSize;
    uint8_t FIFOBuffer[64];
    
    Quaternion q;           // [w, x, y, z]         四元数
    VectorFloat gravity;    // [x, y, z]            重力向量
    float ypr[3];          // [yaw, pitch, roll]   偏航/俯仰/滚动角
    
    bool isMonitoring;
    float lastPitch;
    float lastRoll;
    unsigned long lastTime;
    bool fallenState;
    unsigned long fallStartTime;
    unsigned long lastFallAlert;
    unsigned long stableStartTime; // 新增：姿态恢复稳定的起始时间
    
    // 将const float改为constexpr float，调整稳定阈值，添加恢复持续时间阈值
    static constexpr float FALL_ANGLE_THRESHOLD = 60.0;    // 跌倒角度阈值（度）
    static constexpr float FALL_RATE_THRESHOLD = 45.0;     // 角度变化速率阈值（度/秒）
    static constexpr float STABLE_ANGLE_THRESHOLD = 40.0;  // 稳定（站立）角度阈值（度），应小于FALL_ANGLE_THRESHOLD
    static const unsigned long ALERT_INTERVAL = 2000;  // 报警间隔（毫秒）
    // static const unsigned long RECOVERY_TIMEOUT = 5000; // 恢复检测时间（毫秒） - 不再使用旧逻辑
    static const unsigned long RECOVERY_DURATION_THRESHOLD = 3000; // 姿态恢复稳定持续时间阈值（毫秒）
    
    FallDetectionCallback fallCallback;
};

#endif // FALL_DETECTION_H