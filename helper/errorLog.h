#pragma once // 建議加上 pragma once 防止重複引用
#include <string>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <filesystem> // C++17 用於建立目錄
#include "timeUtil.h"     // 引用你的 time.h 以使用 now_hms_nano()

using namespace std;

inline void logErrorToFile(const string& msg) {
    string logDir = "./log";
    string logFile = logDir + "/errorLog.txt";

    // 1. 確保目錄存在 (C++17)
    try {
        if (!filesystem::exists(logDir)) {
            filesystem::create_directory(logDir);
        }
    } catch (const exception& e) {
        cerr << "Failed to create log directory: " << e.what() << endl;
        return;
    }

    // 2. 開啟檔案 (附加模式)
    ofstream outfile(logFile, ios::app);
    if (outfile.is_open()) {
        // 寫入格式: [時間] 訊息
        outfile << "[" << now_hms_nano() << "] " << msg << endl;
        outfile.close();
    } else {
        cerr << "Unable to open error log file: " << logFile << endl;
    }
}

inline void errorLog(string msg){
    logErrorToFile(msg);
    for (int i = 0; i < 1; i++) {
        cout << " ERROR OCCUR: " << msg << endl;
        // terminate();
        // sleep(1);
    }
    
}