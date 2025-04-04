#ifndef ARDUINO_FS_H
#define ARDUINO_FS_H

#include <Arduino.h>

// 声明index_html变量以供web_server.cpp使用
extern char index_html[65536];

// 初始化文件系统并加载HTML文件
bool initFS();

#endif // ARDUINO_FS_H 