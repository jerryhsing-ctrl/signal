#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>

struct TickNode {
    long long timestamp;
    long long cumulativeQty;
};

class LinearVolumeTracker {
private:
    

public:
    // 儲存行情數據
    std::unordered_map<std::string, std::vector<TickNode>> dataStore;
    
    // 儲存查詢遊標 (Cursor)：紀錄該 Symbol 上次查到第幾個 Index
    std::unordered_map<std::string, size_t> queryCursors;

    // O(1) - 建立資料
    // 這裡我們直接存 "累積量"，比 Query 時才累加更有效率
    void on_tick(const std::string& symbol, long long timeStamp, long long qty) {
        std::vector<TickNode>& history = dataStore[symbol];
        
        long long currentTotal = 0;
        if (!history.empty()) {
            currentTotal = history.back().cumulativeQty;
        }

        history.push_back({timeStamp, currentTotal + qty});
    }

    // Amortized O(1) - 查詢
    // 利用 "上次查到的位置" 繼續往後找
    long long query(const std::string& symbol, long long timeStamp) {
        auto it = dataStore.find(symbol);
        if (it == dataStore.end() || it->second.empty()) {
            return 0;
        }

        const std::vector<TickNode>& history = it->second;
        
        // 取得該 Symbol 目前的遊標位置 (若沒查過，預設為 0)
        size_t& currentIndex = queryCursors[symbol]; 

        // --- 核心邏輯：雙指針 (Sliding Window) ---
        // 只要 "下一個 Tick" 的時間還小於等於 "查詢時間"，就把指針往右移
        // 因為 query 時間遞增，我們永遠不需回頭，currentIndex 只會變大
        while (currentIndex + 1 < history.size() && 
               history[currentIndex + 1].timestamp <= timeStamp) {
            currentIndex++;
        }

        // 邊界檢查：如果第一筆資料的時間就已經大於查詢時間，那累積量是 0
        if (history[currentIndex].timestamp > timeStamp) {
            return 0;
        }

        // 回傳指針當前位置的累積量
        return history[currentIndex].cumulativeQty;
    }
};