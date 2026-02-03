#pragma once

#include <vector>
#include <unordered_map>

// ==========================================
// 選項 A: 陣列實作 (VectorLookup)
// ------------------------------------------
// 優點: 速度最快 O(1)，無 Hash 計算
// 缺點: 如果 user_id 很大 (例如 9999999) 但策略很少，會浪費記憶體
// 適用: user_id 是連續整數 (0 ~ N)
// ==========================================
template <typename T>
class VectorLookup {
private:
    std::vector<T*> table_;

public:
    // 預先分配空間 (Max ID)
    void init(size_t max_id_range) {
        table_.resize(max_id_range + 1, nullptr);
    }

    void insert(int id, T* ptr) {
        if (id >= table_.size()) {
            // 在 HFT 通常不建議這裡 resize，最好 init 就夠大
            // 但為了安全可以加檢查
            return; 
        }
        table_[id] = ptr;
    }

    T* get(int id) {
        if (id >= table_.size()) return nullptr;
        return table_[id];
    }

    void remove(int id) {
        if (id < table_.size()) {
            table_[id] = nullptr;
        }
    }
    
    void clear() {
        // 快速清空，但不釋放記憶體
        std::fill(table_.begin(), table_.end(), nullptr);
    }

    size_t count(size_t id) const {
        // 1. 邊界檢查
        if (id >= table_.size()) return 0;
        // 2. 空值檢查 (必須不是 nullptr 才算存在)
        return (table_[id] != nullptr) ? 1 : 0;
    }
};

// ==========================================
// 選項 B: 雜湊表實作 (HashLookup)
// ------------------------------------------
// 優點: 省記憶體，支援稀疏 ID
// 缺點: 速度較慢 (Hash計算 + 碰撞處理)，有記憶體分配
// 適用: user_id 很大且不連續，或者字串型別
// ==========================================
template <typename T>
class HashLookup {
private:
    std::unordered_map<int, T*> table_;

public:
    void init(size_t expected_count) {
        table_.reserve(expected_count);
    }

    void insert(int id, T* ptr) {
        table_[id] = ptr;
    }

    T* get(int id) {
        auto it = table_.find(id);
        if (it == table_.end()) return nullptr;
        return it->second;
    }

    void remove(int id) {
        table_.erase(id);
    }
    
    void clear() {
        table_.clear();
    }

    size_t count(int id) const {
        return table_.count(id);
    }
};