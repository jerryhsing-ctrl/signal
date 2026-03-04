#pragma once


#include <iostream>
#include <string>
#include <deque>
#include <unordered_map>

class NumTracker {
private:
    long long window_size_us; // Window 時間長度 (微秒)
    
    // Key: Symbol (商品代碼)
    // Value: 交易時間戳記的雙向佇列 (Deque)
    std::unordered_map<std::string, std::deque<long long>> symbol_trades;

public:
    // Constructor: 設定 Window 大小
    NumTracker(long long window_us) : window_size_us(window_us) {}

    /**
     * 更新系統時間，加入新交易，並回傳該 Symbol 在 Window 內的總筆數
     * * @param symbol    商品代碼 (e.g., "BTCUSDT")
     * @param timestamp 交易時間 (微秒)
     * @return          該 Symbol 在過去 window_size_us 內的交易筆數
     */
    size_t on_tick(const std::string& symbol, long long timestamp) {
        // 1. 取得或建立該 Symbol 的 Queue
        // 使用 reference 避免 copy，這是 C++ 效能關鍵
        auto& timestamps = symbol_trades[symbol];

        // 2. 加入最新的交易時間 (假設時間是單調遞增的)
        timestamps.push_back(timestamp);

        // 3. 計算過期門檻
        // 任何小於等於 cutoff 的時間點都需要被移除
        // 例如：現在 1000, window 100 -> 900(含)以前的都過期
        long long cutoff = timestamp - window_size_us;

        // 4. 清理過期資料 (Sliding Window)
        // 由於時間有序，過期的必定在 front，直到遇到一個沒過期的就停止
        while (!timestamps.empty() && timestamps.front() <= cutoff) {
            timestamps.pop_front();
        }

        // 5. 回傳當前 Window 內的剩餘筆數
        return timestamps.size();
    }
};