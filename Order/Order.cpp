#include "Order.h"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <typeinfo>
#include "errorLog.h"

using namespace std;

Order::Order() {
    logFileA.open("./log/order_log_A.txt", ios::app | ios::in);
    if (!logFileA.is_open()) {
        cerr << "Failed to open order_log_A.txt" << endl;
    }
    logFileB.open("./log/order_log_B.txt", ios::app | ios::in);
    if (!logFileB.is_open()) {
        cerr << "Failed to open order_log_B.txt" << endl;
    }
}

void Order::trigger(IndexData &idx, format6Type *f6, int entry_idx, SIGNAL_TYPE signal_type) {
    // cout << "Order::trigger" << endl;
    
    // 選擇相應的日誌文件
    ofstream& logFile = (signal_type == SIGNAL_A) ? logFileA : logFileB;
    
    if (logFile.is_open() && !f6->symbol.empty()) {
            // 檢查symbol長度是否合理

        
        // 獲取當前時間
        auto now = chrono::system_clock::now();
        auto time_t_now = chrono::system_clock::to_time_t(now);
        auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
        
        // 寫入symbol和當前時間
        logFile << "Symbol: " << f6->symbol
                << " | Time: " << f6->matchTimeStr  
                // << " | Time: " << put_time(localtime(&time_t_now), "%Y-%m-%d %H:%M:%S")
                // << "." << setfill('0') << setw(3) << ms.count() 
                // << " | delay: " << time_diff(f6->time, get_times()) << " ns"
                << "\n";
        logFile.flush();
    }

    // f6->record("entry_idx", entry_idx);
    // f6->record("entry_idx: " + to_string(entry_idx));
}