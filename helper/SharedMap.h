#pragma once

#include <iostream>
#include <map>
#include <shared_mutex>
#include <optional>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <functional>
#include "errorLog.h"

template<typename K, typename V>
class SharedMap {
private:
    std::map<K, V> map_;
    mutable std::shared_mutex mutex_; // mutable 允許 const 方法也鎖住

public:
    // 插入或更新
    void put(const K& key, const V& value) {
        std::unique_lock lock(mutex_); // 寫鎖（獨佔）
        map_[key] = value;
    }

    // 查詢
    std::optional<V> get(const K& key) const {
        std::shared_lock lock(mutex_); // 讀鎖（共享）
        auto it = map_.find(key);
        if (it != map_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    // 判斷是否存在
    bool contains(const K& key) const {
        std::shared_lock lock(mutex_);
        return map_.find(key) != map_.end();
    }

    // 移除
    void erase(const K& key) {
        std::unique_lock lock(mutex_);
        map_.erase(key);
    }

    // 取得大小
    size_t size() const {
        std::shared_lock lock(mutex_);
        return map_.size();
    }
	
	void forEach(const std::function<void(const K&, const V&)>& func) const {
		std::shared_lock lock(mutex_);
		for (const auto& [key, value] : map_) {
			func(key, value);
		}
	}

    void modifyForEach(const std::function<void(const K&, V&)>& func) {
        // 3. ⚠️ 重要：必須改用 unique_lock (寫鎖)，否則多執行緒會崩潰
        std::unique_lock lock(mutex_); 
        
        // 4. 這裡的 auto& [key, value] 會取得可修改的 value
        for (auto& [key, value] : map_) {
            func(key, value);
        }
    }
    bool modify(const K& key, const std::function<void(V&)>& modifier) {
        // 因為要修改內容，必須使用 unique_lock (寫鎖)
        std::unique_lock lock(mutex_);
        
        auto it = map_.find(key);
        if (it != map_.end()) {
            // 在鎖的保護範圍內執行修改邏輯
            modifier(it->second); 
            return true;
        }
        else {
            errorLog("no this key in ShareMap");
        }
        return false;
    }
};