#pragma once
#include <deque>
#include <utility>
#include <iostream>
#include <stdexcept>
#include <deque>
#include <utility>
#include <iostream>
#include "errorLog.h"

// T_Time: 時間型別 (e.g., double, int64_t)
// T_Val:  數值型別 (e.g., double, int)
template <typename T_Time, typename T_Val>
class RollingLow {
private:
    // 儲存 Pair: <時間戳, 數值>
    std::deque<std::pair<T_Time, T_Val>> window;
    T_Time duration; // 視窗長度 (例如 50.0)

public:
    // 預設建構子
    RollingLow() : duration(0) {}

    // 建構子：設定視窗長度
    RollingLow(T_Time d) : duration(d) {}

    // 設定視窗長度
    void setDuration(T_Time d) {
        duration = d;
    }

    // 更新資料：傳入當下時間與數值
    void update(T_Time currentTime, T_Val value) {
        // 1. 清理過期數據 (從隊首 Front 移除)
        // 條件：如果 (當前時間 - 記錄時間) > 視窗長度，則過期
        while (!window.empty() && (currentTime - window.front().first > duration)) {
            window.pop_front();
        }

        // 2. 維護單調性 (從隊尾 Back 移除)
        // 核心邏輯：如果新進來的數值比隊尾的數值更小(或相等)，
        // 那麼隊尾那個舊數值就沒有存在的意義了 (因為新數值更小且會活得更久)
        while (!window.empty() && window.back().second >= value) {
            window.pop_back();
        }

        // 3. 存入新數據
        window.push_back({currentTime, value});
    }

    // 取得當前視窗內的最小值
    T_Val getLow() const {
        if (window.empty()) {
            errorLog("Window is empty, no data available.");
            throw std::runtime_error("Window is empty, no data available.");
        }
        // 由於單調性，隊首永遠是最小值
        return window.front().second;
    }

    // 檢查是否有資料
    bool empty() const {
        return window.empty();
    }
    
    // 重置
    void clear() {
        window.clear();
    }
};

template <typename T_Time, typename T_Val>
class RollingSum {
private:
    // 儲存 Pair: <時間戳, 數值>
    // 只需要單純的 deque，不需要單調性
    std::deque<std::pair<T_Time, T_Val>> history;
    
    T_Time duration;   // 視窗長度
    T_Val currentSum;  // 快取當前的總和

public:
    // 預設建構子
    RollingSum() : duration(0), currentSum(0) {}

    // 建構子：初始化視窗長度與總和
    RollingSum(T_Time d) : duration(d), currentSum(0) {}

    // 設定視窗長度
    void setDuration(T_Time d) {
        duration = d;
    }

    // 更新資料
    void update(T_Time currentTime, T_Val value) {
        // 1. 加入新數據
        history.push_back({currentTime, value});
        currentSum += value;

        // 2. 移除過期數據
        // 條件：如果 (當前時間 - 最早記錄時間) > 視窗長度
        while (!history.empty() && (currentTime - history.front().first > duration)) {
            // 從總和中減去即將移除的數值
            currentSum -= history.front().second;
            // 移除隊首
            history.pop_front();
        }
        
        // [防呆機制] 如果因為浮點數誤差導致隊列空了但 sum 不為 0，強制歸零
        if (history.empty()) {
            currentSum = 0;
        }
    }

    // 取得當前總和
    T_Val getSum() const {
        return currentSum;
    }
    
    // 取得當前視窗內的樣本數 (可選功能)
    size_t getCount() const {
        return history.size();
    }

    // 重置
    void clear() {
        history.clear();
        currentSum = 0;
    }
};