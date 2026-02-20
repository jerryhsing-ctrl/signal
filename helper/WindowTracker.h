#pragma once

#include <vector>
#include <cstdint>
#include <iostream>
#include <cstring> 
#include "errorLog.h"

constexpr size_t BUFFER_SIZE = 16384; 
constexpr size_t BUFFER_MASK = BUFFER_SIZE - 1;

struct TickData {
    int64_t timestamp;
    int64_t accum_vol;
};

struct WindowCursor {
    int64_t window_duration; 
    size_t tail_idx;         
    int64_t current_value;   
};

class MultiWindowTracker {
private:
    TickData ring_buffer_[BUFFER_SIZE];
    size_t head_ = 0; 
    int64_t total_volume_so_far_ = 0;
    std::vector<WindowCursor> windows_;

public:
    // 1. 【Constructor】變回預設建構子
    // 只做最基本的記憶體清零 (只會執行一次)
    MultiWindowTracker() {
        std::memset(ring_buffer_, 0, sizeof(ring_buffer_));
        // windows_ 在這裡不需要做任何事，等待 init
    }

    // 2. 【Init】取代原本 Constructor 的功能
    // 支援 Object Pool 重複使用：每次 acquire 時呼叫
    void init(const std::vector<int64_t>& durations) {
        // A. 重置計算狀態
        total_volume_so_far_ = 0;
        head_ = 0;

        // B. 設定視窗 (重用 vector 記憶體)
        // std::vector::clear() 只是把 size 設為 0，不會釋放 capacity
        // 所以如果這物件被反覆使用，這裡完全沒有記憶體配置 (Zero-Allocation)
        windows_.clear();
        
        // 確保容量足夠 (如果上次用了 3 個，這次用 2 個，這行不會做事)
        if (windows_.capacity() < durations.size()) {
            windows_.reserve(durations.size());
        }

        // 填入新的視窗設定
        for (int64_t dur : durations) {
            windows_.push_back({dur, 0, 0});
        }
        
        // C. (選擇性) 如果你很介意髒數據，可以在這裡再 memset 一次
        // 但基於邏輯邊界原理，這裡其實不需要清空 ring_buffer_
    }

    // 寫入新數據 (不變)
    void on_tick(int64_t timestamp_us, int64_t quantity) {
        total_volume_so_far_ += quantity;
        
        size_t idx = head_ & BUFFER_MASK; 
        ring_buffer_[idx].timestamp = timestamp_us;
        ring_buffer_[idx].accum_vol = total_volume_so_far_;

        for (auto& win : windows_) {
            int64_t cutoff_time = timestamp_us - win.window_duration;
            while (true) {
                if (win.tail_idx == head_) break;
                size_t tail_real_idx = win.tail_idx & BUFFER_MASK;
                if (ring_buffer_[tail_real_idx].timestamp < cutoff_time) {
                    win.tail_idx++;
                } else {
                    break;
                }
            }
            int64_t tail_prev_vol = 0;
            if (win.tail_idx > 0) {
                 size_t prev_idx = (win.tail_idx - 1) & BUFFER_MASK;
                 tail_prev_vol = ring_buffer_[prev_idx].accum_vol;
            }
            win.current_value = total_volume_so_far_ - tail_prev_vol;
        }
        head_++;
    }

    int32_t getIdx(int64_t duration) const {
        for (size_t i = 0; i < windows_.size(); i++) {
            if (windows_[i].window_duration == duration) {
                return static_cast<int32_t>(i);
            }
        }
        errorLog("window duration not found");
        return -1; // not found
    }

    // 讀取結果 (不變)
    inline int64_t get(size_t index) const {
        if (index >= windows_.size() || index < 0) {
            errorLog("window 裡的 index 越界");
            return 0;
        }
        return windows_[index].current_value;
    }

    // 重置數據但保留設定 (盤中斷線重連用)
    void clear() {
        total_volume_so_far_ = 0;
        head_ = 0;
        for (auto& win : windows_) {
            win.tail_idx = 0;
            win.current_value = 0;
        }
    }
};

// --- 搭配 ObjectPool 使用範例 ---
// int main() {
//     // 1. 模擬從 Pool 拿出來 (Constructor 已經跑過了)
//     MultiWindowTracker* tracker = new MultiWindowTracker();

//     // 2. 【初始化】設定這次要追蹤 500us 和 1000us
//     tracker->init({500, 1000});

//     tracker.on_tick(100, 10);
//     std::cout << "Val: " << tracker->get(0) << "\n";

//     // ... 任務結束，歸還 Pool ...

//     // 3. 【重複使用】下次拿出來，這次要追蹤不同的視窗 (例如 3000us)
//     // init 會清空舊的設定，套用新的
//     tracker->init({3000}); 
    
//     tracker.on_tick(200, 50);
//     std::cout << "Val: " << tracker->get(0) << "\n"; // 這裡是 3000us 的結果

//     delete tracker;
//     return 0;
// }