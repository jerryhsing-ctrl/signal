#pragma once
#include <map>
#include <functional>
#include <vector>
#include <string>
#include <unordered_map>

class GroupRank {
private:
    // key: 漲幅(double), value: group name
    // 使用 greater 降序排列，begin() 就是漲幅最大的
    

    // 反查：group name -> 當前漲幅，用來做更新時移除舊值
    std::unordered_map<std::string, double> group_gain;

public:
    std::map<double, std::string, std::greater<double>> rank_map;
    // 更新某族群的漲幅
    void on_tick(const std::string& group, double gain) {
        // 如果這個族群已有紀錄，先移除舊的
        auto it = group_gain.find(group);
        if (it != group_gain.end()) {
            rank_map.erase(it->second);
        }
        // 插入新的漲幅
        group_gain[group] = gain;
        rank_map[gain] = group;
    }

    // 查詢漲幅最高的前 N 個族群 (預設 5)
    std::vector<std::pair<std::string, double>> query(int n = 5) const {
        std::vector<std::pair<std::string, double>> result;
        result.reserve(n);
        int count = 0;
        for (auto it = rank_map.begin(); it != rank_map.end() && count < n; ++it, ++count) {
            result.push_back({it->second, it->first});  // {group, gain}
        }
        return result;
    }

    // 刪除某族群
    void erase(const std::string& group) {
        auto it = group_gain.find(group);
        if (it != group_gain.end()) {
            rank_map.erase(it->second);
            group_gain.erase(it);
        }
    }

    // 查詢某族群排名第幾名 (1-based)，找不到回傳 -1
    int getRank(const std::string& group) const {
        auto it = group_gain.find(group);
        if (it == group_gain.end()) return -1;

        int rank = 1;
        for (auto it = rank_map.begin(); it != rank_map.end(); ++it, ++rank) {
            if (it->second == group) return rank;
        }
        return -1;
    }

    // 查詢某族群是否在前 N 名
    bool isTopN(const std::string& group, int n = 5) const {
        auto it = group_gain.find(group);
        if (it == group_gain.end()) return false;

        // double gain = it->second;
        int count = 0;
        for (auto it = rank_map.begin(); it != rank_map.end() && count < n; ++it, ++count) {
            if (it->second == group) return true;
        }
        return false;
    }
};
