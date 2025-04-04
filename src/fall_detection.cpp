#include "fall_detection.h"

FallDetection::FallDetection() : 
    DMPReady(false),
    devStatus(0),
    packetSize(0),
    isMonitoring(false),
    lastPitch(0),
    lastRoll(0),
    lastTime(0),
    fallenState(false),
    fallStartTime(0),
    lastFallAlert(0),
    stableStartTime(0),
    fallCallback(nullptr) {
}

bool FallDetection::begin() {
    Serial.println("初始化MPU6050...");
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
        Wire.setClock(400000);  // 400kHz I2C clock
        delay(100);  // 添加延时确保I2C总线稳定
    #endif

    mpu.initialize();
    
    // 添加重试机制
    int retryCount = 0;
    const int maxRetries = 3;
    
    while (!mpu.testConnection() && retryCount < maxRetries) {
        Serial.printf("MPU6050连接失败，正在重试 (%d/%d)...\n", retryCount + 1, maxRetries);
        delay(1000);
        retryCount++;
    }
    
    if (retryCount >= maxRetries) {
        Serial.println("MPU6050连接失败，请检查接线和I2C地址!");
        return false;
    }
    
    Serial.println("MPU6050连接成功");

    devStatus = mpu.dmpInitialize();
    if (devStatus == 0) {
        Serial.println("开始校准MPU6050...");
        calibrate();
        mpu.setDMPEnabled(true);
        DMPReady = true;
        packetSize = mpu.dmpGetFIFOPacketSize();
        Serial.println("MPU6050初始化完成!");
        return true;
    }
    Serial.println("DMP初始化失败!");
    return false;
}

void FallDetection::calibrate() {
    mpu.CalibrateAccel(6);
    mpu.CalibrateGyro(6);
}

void FallDetection::readSensorData() {
    if (mpu.dmpGetCurrentFIFOPacket(FIFOBuffer)) {
        mpu.dmpGetQuaternion(&q, FIFOBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    }
}

void FallDetection::startMonitoring() {
    isMonitoring = true;
    Serial.println("跌倒监测已启动");
}

void FallDetection::stopMonitoring() {
    isMonitoring = false;
    Serial.println("跌倒监测已停止");
}

// 修改：直接返回成员变量中缓存的数据
bool FallDetection::getCurrentData(Quaternion &outQ, VectorFloat &outGravity, float (&outYpr)[3]) {
    if (!DMPReady) return false;
    
    // 复制缓存的数据到输出参数
    outQ = q;
    outGravity = gravity;
    outYpr[0] = ypr[0];
    outYpr[1] = ypr[1];
    outYpr[2] = ypr[2];
        
    return true; // 假设数据总是有效的（如果 DMPReady）
}

void FallDetection::update() {
    if (!DMPReady || !isMonitoring) return;
    static unsigned long lastPrintTime = 0;
    static unsigned long lastDataUpdateTime = 0;
    const unsigned long DATA_UPDATE_INTERVAL = 100;  // 数据更新间隔（毫秒）

    // 从FIFO读取数据并存储到成员变量 q, gravity, ypr
    if (mpu.dmpGetCurrentFIFOPacket(FIFOBuffer)) {
        mpu.dmpGetQuaternion(&q, FIFOBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
        
        float currentPitch = ypr[1] * 180/M_PI;
        float currentRoll = ypr[2] * 180/M_PI;
        unsigned long currentTime = millis();

        // 定期更新和显示传感器数据
        if (currentTime - lastDataUpdateTime >= DATA_UPDATE_INTERVAL) {
            // Serial.printf("姿态数据 - Pitch: %.2f°, Roll: %.2f°\n", currentPitch, currentRoll); // 暂时注释掉过于频繁的打印
            lastDataUpdateTime = currentTime;
        }

        // 状态输出
        if (currentTime - lastPrintTime >= 1000) { // 减少打印频率
            if (fallenState) {
                Serial.println("⚠️ 警告：老人处于倒地状态！");
                Serial.println("持续时间：" + String((currentTime - fallStartTime) / 1000) + "秒");
            } else {
                // Serial.println("✓ 老人状态正常"); // 正常状态不必频繁打印
            }
            lastPrintTime = currentTime;
        }
        
        if (lastTime != 0) {
            float deltaTime = (currentTime - lastTime) / 1000.0;
            // 避免 deltaTime 过小导致速率异常
            if (deltaTime < 0.01) deltaTime = 0.01;
            float pitchRate = abs(currentPitch - lastPitch) / deltaTime;
            float rollRate = abs(currentRoll - lastRoll) / deltaTime;
            
            bool isAbnormalAngle = (abs(currentPitch) > FALL_ANGLE_THRESHOLD || 
                                   abs(currentRoll) > FALL_ANGLE_THRESHOLD);
            bool isHighRate = (pitchRate > FALL_RATE_THRESHOLD || 
                             rollRate > FALL_RATE_THRESHOLD);
            bool isStableAngle = (abs(currentPitch) <= STABLE_ANGLE_THRESHOLD && 
                                  abs(currentRoll) <= STABLE_ANGLE_THRESHOLD);
            
            // 跌倒检测逻辑
            if (!fallenState && isAbnormalAngle && isHighRate) {
                fallenState = true;
                fallStartTime = currentTime;
                stableStartTime = 0; // 重置稳定开始时间
                Serial.println("⚠️ 警告：检测到老人跌倒！");
                Serial.println("需要立即救助！");
                if (fallCallback) {
                    fallCallback("老人跌倒警报！");
                }
            }
            // 持续倒地状态与恢复逻辑
            else if (fallenState) {
                if (!isStableAngle) { // 如果姿态不稳（仍处于倾斜）
                    stableStartTime = 0; // 重置稳定开始时间
                    if (currentTime - lastFallAlert > ALERT_INTERVAL) {
                        if (fallCallback) {
                            fallCallback("老人持续处于倒地状态，请立即查看！");
                        }
                        Serial.println("⚠️ 严重警告：老人仍未得到救助！");
                        Serial.println("倒地持续时间：" + String((currentTime - fallStartTime) / 1000) + "秒");
                        lastFallAlert = currentTime;
                    }
                } else { // 如果姿态稳定（在 STABLE_ANGLE_THRESHOLD 内）
                    if (stableStartTime == 0) { // 刚开始稳定
                        stableStartTime = currentTime;
                        Serial.println("检测到姿态恢复稳定，开始计时...");
                    } else { // 已经稳定了一段时间
                        if (currentTime - stableStartTime > RECOVERY_DURATION_THRESHOLD) { // 稳定时间足够长
                            fallenState = false;
                            stableStartTime = 0; // 重置
                            fallStartTime = 0; // 重置
                            if (fallCallback) {
                                fallCallback("老人已恢复正常姿态");
                            }
                            Serial.println("✓ 老人已恢复正常姿态");
                        }
                    }
                }
            }
        }
        
        lastPitch = currentPitch;
        lastRoll = currentRoll;
        lastTime = currentTime;
    }
}