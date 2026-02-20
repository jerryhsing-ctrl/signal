#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
#include <set>
#include <vector>
#include <utility>

// 使用 Pair 儲存 (Volume, Symbol)，Set 會自動依據 Pair 的第一個元素 (Volume) 排序
using StockEntry = std::pair<long long, std::string>;

class TopKVolumeTracker {
private:
    size_t K;

    // 儲存全市場所有股票的當前累積量
    std::unordered_map<std::string, long long> all_volumes;

    // 核心優化：使用 std::set 取代 vector
    // set 自動排序，begin() 永遠是最小值 (門檻值)
   

    // 快速查找某支股票是否在榜內
    std::unordered_map<std::string, bool> is_in_top;

public:
    std::set<StockEntry> top_list;
    TopKVolumeTracker(size_t k_val = 200) : K(k_val) {}
    
    bool inPool(const std::string& symbol) const {
        auto it = is_in_top.find(symbol);
        return (it != is_in_top.end() && it->second);
    }
    // 回傳: {NewEntrant, Leaver}
    std::pair<std::string, std::string> on_tick(const std::string& symbol, long long volume_delta) {
        // 1. 取得舊的 Volume (如果存在)
        long long old_vol = 0;
        if (all_volumes.count(symbol)) {
            old_vol = all_volumes[symbol];
        }

        // 2. 更新總量
        all_volumes[symbol] += volume_delta;
        long long new_vol = all_volumes[symbol];

        // 3. 邏輯判斷
        // --- CASE A: 該股票已經在榜內 ---
        if (is_in_top[symbol]) {
            // 因為 Volume 變了，必須先從 Set 移除舊的，再插入新的
            // std::set 的移除與插入都是 O(log K)
            top_list.erase({old_vol, symbol}); 
            top_list.insert({new_vol, symbol});
            
            // 雖然數值變了，但它原本就在榜內，所以沒有人進出
            return {"", ""};
        }

        // --- CASE B: 該股票目前在榜外 ---
        
        // B1: 榜單還沒滿 -> 直接進榜
        if (top_list.size() < K) {
            top_list.insert({new_vol, symbol});
            is_in_top[symbol] = true;
            return {symbol, ""}; // 只有進，沒有出
        }

        // B2: 榜單已滿 -> 挑戰門檻 (最小值)
        // set 的 begin() 就是最小值
        const auto& min_entry = *top_list.begin();
        long long threshold = min_entry.first;

        if (new_vol > threshold) {
            // --- CASE C: 踢掉最後一名 (擠進榜) ---
            std::string leaver = min_entry.second;
            std::string entrant = symbol;

            // 移除舊的最小值 (Min)
            is_in_top.erase(leaver); // 從 map 標記移除
            top_list.erase(top_list.begin()); // 從 set 移除，O(1) 或 O(log K)

            // 插入新的強者
            top_list.insert({new_vol, symbol});
            is_in_top[symbol] = true;

            return {entrant, leaver};
        }

        // --- CASE D: 在榜外且量不夠 ---
        return {"", ""};
    }
    
    // 用於 Debug 或檢查目前狀態
    const std::set<StockEntry>& getTopList() const {
        return top_list;
    }
};