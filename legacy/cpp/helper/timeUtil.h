#pragma once


#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <chrono>
#include <time.h>
#include <x86intrin.h> // GCC/Clang 內建指令集
#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#include <atomic>
#include <string>

using timeType = std::chrono::time_point<std::chrono::system_clock>;
inline timeType zero_time = std::chrono::system_clock::from_time_t(0);
inline timeType get_times() {
    // struct timespec ts;
    // // CLOCK_MONOTONIC_RAW 不受 NTP 校時影響，更適合計算純時間差
    // clock_gettime(CLOCK_MONOTONIC, &ts); 
    // return (long long)ts.tv_sec * 1000000000 + ts.tv_nsec;
    
    return std::chrono::system_clock::now();
}

inline long long time_diff(timeType start, timeType end) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

inline std::string now_hms_nano() {
    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);  // 牆鐘時間，含奈秒

    std::tm tm{};
    localtime_r(&ts.tv_sec, &tm);        // 本機時區

    std::ostringstream os;
    os << std::setfill('0')
       << std::setw(2) << tm.tm_hour << ':'
       << std::setw(2) << tm.tm_min  << ':'
       << std::setw(2) << tm.tm_sec  << '.'
       << std::setw(9) << ts.tv_nsec;       
    //    << std::setw(9) << ts.tv_nsec;   // 奈秒補 9 位
    return os.str();
}

// inline timeType now_hms_nano() {
//     return std::chrono::steady_clock::now();
// }

class TimeLogger {
public:
    std::string seq;
    std::string strategy;
    std::string quote;
    std::string send;
    std::string get() {
        return "seq=" + seq + ", strategy=" + strategy + ", quote=" + quote + ", send=" + send;
    }
    
};

// inline void recordTime(timeType start, timeType end) {
//     static int cnt = 0;
//     static long long records[21];
//     static int target = 5;
//     records[cnt++] = time_diff(start, end);
//     if (cnt % target == 0) {
        

//         for (int i = 0; i < cnt; i++)
//             std::cout << " 時間: " << records[i] << " 微秒\n";
//         std::cout << "------------------------\n";
//         cnt = 0;
//     }
// }



// inline void recordTime(timeType start, timeType end) {
//     // ---------------------------------------------------------
//     // 1. 設定靜態檔案物件 (只會在第一次呼叫時執行初始化/開啟檔案)
//     // ---------------------------------------------------------
//     static std::ofstream logFile("./log/latency_record.log", std::ios::out | std::ios::app);
    
//     // ---------------------------------------------------------
//     // 2. 設定靜態緩衝區 (所有呼叫者共用這塊記憶體)
//     // ---------------------------------------------------------
//     static const int BATCH_SIZE = 5; // 累積 100 筆再寫，減少 I/O
//     static long long records[BATCH_SIZE];
//     static int cnt = 0;

//     // ---------------------------------------------------------
//     // 3. 執行邏輯
//     // ---------------------------------------------------------
    
//     // 存入 Buffer (純記憶體操作，極快)
//     records[cnt++] = time_diff(start, end);

//     // Buffer 滿了才寫入檔案
//     if (cnt >= BATCH_SIZE) {
//         if (logFile.is_open()) {
//             for (int i = 0; i < cnt; i++) {
//                 // 使用 '\n' 而不是 endl，避免強制 flush 造成效能低落
//                 logFile << "Time: " << records[i] << " us\n";
//             }
//             // 這裡不需要手動 flush，OS 會自己管理，效能最好。
//             // 除非你擔心程式崩潰資料遺失，才加 logFile.flush();
//         }
//         cnt = 0; // 重置計數器
//     }
// }

inline void recordTime(std::string &seqno, timeType start, timeType end) {
    // ---------------------------------------------------------
    // 1. 設定靜態檔案物件 (只會在第一次呼叫時執行初始化/開啟檔案)
    // ---------------------------------------------------------
    static bool initialized = false;
    static std::ofstream logFile;
    if (!initialized) {
        struct stat st{};
        if (stat("./log", &st) != 0) {
            mkdir("./log", 07777);
        }
        logFile.open("./log/latency_record.log", std::ios::out | std::ios::app);
        if (!logFile.is_open()) {
            std::cerr << "recordTime: cannot open ./log/latency_record.log" << std::endl;
        }
        initialized = true;
    }
    
    // ---------------------------------------------------------
    // 2. 設定靜態緩衝區 (所有呼叫者共用這塊記憶體)
    // ---------------------------------------------------------
    static const int BATCH_SIZE = 1; // 累積 100 筆再寫，減少 I/O
    static long long records[BATCH_SIZE];
    static std::atomic<int> cnt = 0;
    static std::atomic<int> small_cnt = 0;
    // ---------------------------------------------------------
    // 3. 執行邏輯
    // ---------------------------------------------------------
    
    // 存入 Buffer (純記憶體操作，極快)
    records[cnt++] = time_diff(start, end);
    // std::cout << "hi\n";
    // Buffer 滿了才寫入檔案
    if (cnt >= BATCH_SIZE) {
        // std::cout << "writeToFile\n";
        long long total = 0;
        long long small_total = 0;
        
        if (logFile.is_open()) {
            for (int i = 0; i < cnt; i++) {
                // 使用 '\n' 而不是 endl，避免強制 flush 造成效能低落
                // logFile << "Time: " << records[i] << " us\n";
                total += records[i];
                if (records[i] < 1000) { 
                    small_total += records[i];
                    small_cnt++;
                }
            }
            logFile << "seqno = " << seqno << "  average = " << ((double) total / BATCH_SIZE / 1000) << " us\n"; 
            // 這裡不需要手動 flush，OS 會自己管理，效能最好。
            // 除非你擔心程式崩潰資料遺失，才加 logFile.flush();
        }
        logFile.flush();
        cnt = 0; // 重置計數器
        small_cnt = 0;
    }
}

inline void recordTime(std::string &seqno, long long time) {
    // ---------------------------------------------------------
    // 1. 設定靜態檔案物件 (只會在第一次呼叫時執行初始化/開啟檔案)
    // ---------------------------------------------------------
    static bool initialized = false;
    static std::ofstream logFile;
    if (!initialized) {
        struct stat st{};
        if (stat("./log", &st) != 0) {
            mkdir("./log", 07777);
        }
        logFile.open("./log/latency_record.log", std::ios::out | std::ios::app);
        if (!logFile.is_open()) {
            std::cerr << "recordTime: cannot open ./log/latency_record.log" << std::endl;
        }
        initialized = true;
    }
    
    // ---------------------------------------------------------
    // 2. 設定靜態緩衝區 (所有呼叫者共用這塊記憶體)
    // ---------------------------------------------------------
    static const int BATCH_SIZE = 1; // 累積 100 筆再寫，減少 I/O
    static long long records[BATCH_SIZE];
    static std::atomic<int> cnt = 0;
    static std::atomic<int> small_cnt = 0;
    // ---------------------------------------------------------
    // 3. 執行邏輯
    // ---------------------------------------------------------
    
    // 存入 Buffer (純記憶體操作，極快)
    records[cnt++] = time;
    // std::cout << "hi\n";
    // Buffer 滿了才寫入檔案
    if (cnt >= BATCH_SIZE) {
        // std::cout << "writeToFile\n";
        long long total = 0;
        long long small_total = 0;
        
        if (logFile.is_open()) {
            for (int i = 0; i < cnt; i++) {
                // 使用 '\n' 而不是 endl，避免強制 flush 造成效能低落
                // logFile << "Time: " << records[i] << " us\n";
                total += records[i];
                if (records[i] < 1000) { 
                    small_total += records[i];
                    small_cnt++;
                }
            }
            logFile << "seqno = " << seqno << "  average = " << ((double) total / BATCH_SIZE / 1000) << " us\n"; 
            // 這裡不需要手動 flush，OS 會自己管理，效能最好。
            // 除非你擔心程式崩潰資料遺失，才加 logFile.flush();
        }
        logFile.flush();
        cnt = 0; // 重置計數器
        small_cnt = 0;
    }
}