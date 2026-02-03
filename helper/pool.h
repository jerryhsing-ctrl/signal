#pragma once

#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include <utility>
#include <unordered_map>
#include "errorLog.h" // 確保這個路徑存在
#include <string>

// 1. 基礎結構
// class Poolable {
// public:
//     int pool_id = -1;
//     int active_idx = -1;
//     int key = -1;
// };

class Poolable {
public:
    int pool_id = -1;
    int active_idx = -1;
    string key = "";
};

// 2. 極速通用容器 (Raw Array 版本)
template <typename T, size_t Capacity>
class ActiveObjectPool {
private:
    // [重物區] 放在 Heap，避免 Stack Overflow
    std::vector<T> storage_;

    // [輕量區] 放在 Stack/Inline，極速存取
    T* active_objects_[Capacity]; 
    size_t active_count_ = 0;

    int free_indices_[Capacity];
    size_t free_count_ = 0;

public:
    ActiveObjectPool() {
        // 1. 配置倉庫 (一次到位)
        storage_.resize(Capacity);

        // 2. 初始化閒置名單
        for (int i = 0; i < (int)Capacity; ++i) {
            free_indices_[i] = i;
        }
        free_count_ = Capacity;
        active_count_ = 0;
    }

    // [借出]
    template <typename... Args>
    T* acquire(Args&&... args) {
        if (free_count_ == 0) {
            errorLog("ActiveObjectPool 已滿，無法借出更多物件！");
            return nullptr;
        }

        // 1. 從 Free List 拿 ID (Array Pop)
        int idx = free_indices_[--free_count_];

        // 2. 設定物件
        T* obj = &storage_[idx];
        obj->pool_id = idx;
        obj->active_idx = (int)active_count_;
        
        // 3. 原地初始化
        obj->init(std::forward<Args>(args)...);

        // 4. 加入活躍清單 (Array Push)
        active_objects_[active_count_++] = obj;
        
        return obj;
    }

    // [歸還]
    void release(T* obj) {
        int idx_to_remove = obj->active_idx;
        
        // 1. 歸還 ID (Array Push)
        free_indices_[free_count_++] = obj->pool_id;

        // 2. Swap-and-Pop (陣列版)
        size_t last_idx = active_count_ - 1;
        T* last_obj = active_objects_[last_idx];
        
        // 覆蓋當前位置
        active_objects_[idx_to_remove] = last_obj;
        
        // 更新被搬運物件的索引
        last_obj->active_idx = idx_to_remove;
        
        // 縮小尺寸
        active_count_--;
        
        // 重置狀態
        obj->active_idx = -1;
    }

    // [遍歷]
    template <typename Func>
    void for_each(Func&& func) {
        for (size_t i = 0; i < active_count_; ++i) {
            T* obj = active_objects_[i];
            
            // 執行邏輯 (回傳 false 代表要刪除)
            bool keep_alive = func(obj);

            if (!keep_alive) {
                release(obj);
                // 因為 Swap 換了新的人過來，且 i 是無號整數
                // i-- 會變成 MAX_INT，但迴圈的 ++i 會把它加回正確的 index
                i--; 
            }
        }
    }

    void clear() {
        // 1. (選擇性) 重置所有活躍物件的狀態
        // 如果你的 T 物件有狀態需要被清空，可以在這裡做
        for (size_t i = 0; i < active_count_; ++i) {
            T* obj = active_objects_[i];
            obj->active_idx = -1; 
            // obj->reset(); // 如果物件有 reset 方法，也可以呼叫
        }

        // 2. 重置 Free List (回到初始狀態 0 ~ Capacity-1)
        // 直接無腦填回去，比一個個歸還快
        for (int i = 0; i < (int)Capacity; ++i) {
            free_indices_[i] = i;
        }

        // 3. 重置計數器
        free_count_ = Capacity;
        active_count_ = 0;
    }
    
    // 狀態查詢
    size_t size() const { return active_count_; }
    size_t capacity() const { return Capacity; }
    size_t available() const { return free_count_; }
    
    bool full() const { return free_count_ == 0; }
    bool empty() const { return active_count_ == 0; }
};



// template <typename T, size_t Capacity, size_t MaxKey>
// class HashObjectPool {
// private:
//     // [重物區] 實際物件儲存
//     // std::vector<T> storage_;
//     std::unique_ptr<T[]> storage_;

//     // [活躍區] 遍歷用 (Dense Array)
//     T* active_objects_[Capacity]; 
//     size_t active_count_ = 0;

//     // [閒置區] 分配用 (Free List)
//     int free_indices_[Capacity];
//     size_t free_count_ = 0;

//     // 【新增】[查詢區] 隨機存取用 (Sparse Array)
//     // 直接用 Key 當 index 對應到物件指標
//     // 這種 "以空間換時間" 是 HFT 最常見的手段
//     T* lookup_table_[MaxKey]; 

// public:
//     HashObjectPool()
//         : storage_(std::make_unique<T[]>(Capacity)), active_count_(0), free_count_(Capacity)
//     {
//         // 1. 配置倉庫
//         // storage_.resize(Capacity);

//         // 2. 初始化閒置名單
//         for (int i = 0; i < (int)Capacity; ++i) {
//             free_indices_[i] = i;
//         }

//         // 3. 【新增】初始化查找表 (全部設為 nullptr)
//         std::memset(lookup_table_, 0, sizeof(lookup_table_));
//     }

//     // [查詢] (新增功能)
//     // 極速 O(1) 查找，只有一個 Array Access 指令
//     T* get(int key) const {
//         if (key < 0 || key >= (int)MaxKey){ 
//             errorLog("key > 邊界");
//             return nullptr; // 邊界檢查
//         }
//         return lookup_table_[key];
//     }

//     // [借出] 
//     // 注意：必須傳入 key 讓 Pool 註冊
//     template <typename... Args>
//     T* acquire(int key, Args&&... args) {
//         // A. 基本檢查
//         if (free_count_ == 0) return nullptr;
//         if (key < 0 || key >= (int)MaxKey) return nullptr;

//         // B. 【重要】檢查 Key 是否重複
//         // 如果這個 Key 已經存在，你可以決定是回傳舊的(Idempotent) 還是報錯
//         if (lookup_table_[key] != nullptr) {
//             // 策略1: 直接回傳舊的 (如果不允許重複創建)
//             return lookup_table_[key]; 
//         }

//         // C. 正常的 Pool 分配邏輯
//         int idx = free_indices_[--free_count_];
//         T* obj = &storage_[idx];
        
//         obj->pool_id = idx;
//         obj->active_idx = (int)active_count_;
//         obj->key = key; // 【新增】記住自己的 Key

//         // D. 初始化物件
//         obj->init(std::forward<Args>(args)...);

//         // E. 加入活躍清單 (遍歷用)
//         active_objects_[active_count_++] = obj;

//         // F. 【新增】註冊到查找表 (查詢用)
//         lookup_table_[key] = obj;
        
//         return obj;
//     }

//     // [歸還]
//     void release(T* obj) {
//         if (!obj) return;

//         // 1. 【新增】從查找表移除
//         // 這是 O(1)，只要把指標清空
//         if (obj->key >= 0 && obj->key < (int)MaxKey) {
//             lookup_table_[obj->key] = nullptr;
//         }

//         // 2. 標準歸還邏輯 (Swap and Pop)
//         int idx_to_remove = obj->active_idx;
//         free_indices_[free_count_++] = obj->pool_id; // 歸還 ID

//         size_t last_idx = active_count_ - 1;
//         T* last_obj = active_objects_[last_idx];

//         active_objects_[idx_to_remove] = last_obj;
//         last_obj->active_idx = idx_to_remove; // 更新被搬運者的 index

//         active_count_--;

//         // 3. 清除狀態
//         obj->active_idx = -1;
//         obj->key = -1;
//     }

//     // [遍歷] 不變
//     template <typename Func>
//     void for_each(Func&& func) {
//         for (size_t i = 0; i < active_count_; ++i) {
//             T* obj = active_objects_[i];
//             if (!func(obj)) {
//                 release(obj);
//                 i--; 
//             }
//         }
//     }

//     void clear() {
//         // 1. 遍歷活躍名單，精準清除 Lookup Table
//         // 這樣做比 memset(lookup_table_, 0, ...) 快非常多，
//         // 尤其是當 MaxKey 很大但 Active Count 很小時。
//         for (size_t i = 0; i < active_count_; ++i) {
//             T* obj = active_objects_[i];

//             // A. 清空查找表 (只清有用的格子)
//             if (obj->key >= 0 && obj->key < (int)MaxKey) {
//                 lookup_table_[obj->key] = nullptr;
//             }

//             // B. 重置物件內部狀態
//             obj->active_idx = -1;
//             obj->key = -1;
//             // obj->reset(); // 可選
//         }

//         // 2. 重置 Free List (回到初始狀態)
//         for (int i = 0; i < (int)Capacity; ++i) {
//             free_indices_[i] = i;
//         }

//         // 3. 重置計數器
//         free_count_ = Capacity;
//         active_count_ = 0;
//     }
// };


template <typename T, size_t Capacity>
class StringHashPool {
private:
    // [重物區] 實際物件儲存 (連續記憶體，對 Cache 友善)
    std::unique_ptr<T[]> storage_;

    // [活躍區] 遍歷用 (Dense Array)
    T* active_objects_[Capacity]; 
    size_t active_count_ = 0;

    // [閒置區] 分配用 (Free List)
    int free_indices_[Capacity];
    size_t free_count_ = 0;

    // [查詢區] 使用標準庫 (簡單、穩定，但有 allocation)
    std::unordered_map<std::string, T*> lookup_table_;

public:
    StringHashPool()
        : storage_(std::make_unique<T[]>(Capacity)), active_count_(0), free_count_(Capacity)
    {
        // 1. 初始化閒置名單
        for (int i = 0; i < (int)Capacity; ++i) {
            free_indices_[i] = i;
        }
        
        // 2. 預留 Map 空間，避免初期 rehash
        lookup_table_.reserve(Capacity);
    }

    // [查詢] O(1) 平均
    T* get(const std::string& key) const {
        auto it = lookup_table_.find(key);
        if (it != lookup_table_.end()) {
            return it->second;
        }
        return nullptr;
    }

    // [借出]
    template <typename... Args>
    T* acquire(const std::string& key, Args&&... args) {
        // A. 基本檢查
        if (free_count_ == 0) return nullptr;

        // B. 檢查 Key 是否重複 (Idempotent)
        auto it = lookup_table_.find(key);
        if (it != lookup_table_.end()) {
            return it->second; // 已存在，直接回傳舊的
        }

        // C. 從 Pool 分配物件
        int idx = free_indices_[--free_count_];
        T* obj = &storage_[idx];
        
        obj->pool_id = idx;
        obj->active_idx = (int)active_count_;
        
        // 【重要】物件需要記住自己的 Key，以便歸還時從 map 移除
        // 這裡會發生 string copy，是這個簡單版的主要效能開銷
        obj->key = key; 

        // D. 初始化物件
        obj->init(std::forward<Args>(args)...);

        // E. 加入活躍清單 (遍歷用)
        active_objects_[active_count_++] = obj;

        // F. 加入 Map (查詢用)
        lookup_table_[key] = obj;
        
        return obj;
    }

    // [歸還]
    void release(T* obj) {
        if (!obj) return;

        // 1. 從 Map 移除
        // 因為 obj 記住了 key，所以我們可以找到並刪除
        lookup_table_.erase(obj->key);

        // 2. 標準歸還邏輯 (Swap and Pop)
        // 保持活躍陣列緊湊，遍歷速度依然極快
        int idx_to_remove = obj->active_idx;
        free_indices_[free_count_++] = obj->pool_id; // 歸還 ID

        size_t last_idx = active_count_ - 1;
        T* last_obj = active_objects_[last_idx];

        active_objects_[idx_to_remove] = last_obj;
        last_obj->active_idx = idx_to_remove; // 更新被搬運者的 index

        active_count_--;

        // 3. 清除狀態
        obj->active_idx = -1;
        obj->key.clear(); // 清空 string
    }

    // [一鍵清空]
    void clear() {
        // 1. 清空 Map (標準庫會幫忙釋放 Node 記憶體)
        lookup_table_.clear();

        // 2. 重置所有物件狀態
        for (size_t i = 0; i < active_count_; ++i) {
            T* obj = active_objects_[i];
            obj->active_idx = -1;
            obj->key.clear();
        }

        // 3. 重置 Free List
        for (int i = 0; i < (int)Capacity; ++i) {
            free_indices_[i] = i;
        }

        // 4. 重置計數器
        free_count_ = Capacity;
        active_count_ = 0;
    }

    // [遍歷]
    template <typename Func>
    void for_each(Func&& func) {
        for (size_t i = 0; i < active_count_; ++i) {
            T* obj = active_objects_[i];
            bool keep_alive = func(obj);
            if (!keep_alive) {
                release(obj);
                i--; 
            }
        }
    }
    
    size_t size() const { return active_count_; }
};

// 3. 環狀覆寫型 Pool (永遠不會借不到物件)
template <typename T, size_t Capacity>
class CircularObjectPool {
private:
    std::vector<T> storage_;
    size_t head_ = 0; // 當前寫入位置

public:
    CircularObjectPool() {
        storage_.resize(Capacity);
    }

    // [借出] 
    // 特性：永遠不會失敗，永遠回傳物件
    // 如果滿了，它會直接覆蓋掉最舊的那個物件 (Index 0, 1, 2...)
    template <typename... Args>
    T* acquire(Args&&... args) {
        
        // 1. 拿到當前指標
        T* obj = &storage_[head_];
        
        // 2. 紀錄 ID (如果需要 Debug)
        // obj->pool_id = head_; 

        // 3. 原地初始化 (覆蓋舊資料)
        obj->init(std::forward<Args>(args)...);

        // 4. 移動指標 (轉圈圈)
        head_++;
        if (head_ >= Capacity) {
            head_ = 0; // 回到起點，準備覆蓋最舊的
        }

        return obj;
    }
    
    // [清除] 
    // 只是重置指標，不釋放記憶體
    void clear() {
        head_ = 0;
    }
};