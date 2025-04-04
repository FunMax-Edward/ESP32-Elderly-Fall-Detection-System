#include "arduino_fs.h"
#include <SPIFFS.h>

// 定义一个保存HTML内容的字符数组，会在setup时加载
char index_html[65536]; // 足够大的缓冲区，64KB，可以根据实际HTML大小调整

// 初始化文件系统并加载HTML文件
bool initFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS挂载失败！");
        return false;
    }
    
    Serial.println("SPIFFS挂载成功，正在加载网页文件...");
    
    // 加载index.html
    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
        Serial.println("无法打开index.html！");
        return false;
    }
    
    size_t fileSize = file.size();
    if (fileSize > sizeof(index_html) - 1) {
        Serial.println("index.html文件过大，无法加载到内存！");
        file.close();
        return false;
    }
    
    // 读取文件内容到内存
    file.readBytes(index_html, fileSize);
    index_html[fileSize] = '\0'; // 确保以NULL结尾
    
    file.close();
    Serial.println("index.html已加载至内存，大小: " + String(fileSize) + " 字节");
    
    // 检查其他关键文件是否存在
    if (!SPIFFS.exists("/modern.css")) {
        Serial.println("警告: modern.css文件不存在，样式将无法正常显示");
    } else {
        Serial.println("发现modern.css文件");
    }
    
    if (!SPIFFS.exists("/sw.js")) {
        Serial.println("警告: sw.js文件不存在，离线功能将无法使用");
    } else {
        Serial.println("发现sw.js文件，支持离线访问");
    }
    
    return true;
}
