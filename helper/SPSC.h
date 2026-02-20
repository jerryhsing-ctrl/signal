#pragma once

#include <atomic>
#include <vector>
#include <new> // hardware_constructive_interference_size

#include "errorLog.h"

// 獲取 Cache Line 大小 (通常是 64 bytes)
// #ifdef __cpp_lib_hardware_interference_size
//     // 如果支援，直接使用標準庫提供的數值 (它會自動判斷是 64 還是 128)
//     constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
// #else
//     // 如果編譯器太舊 (或 MSVC 某些版本警告)，則 fallback 到 64 (x86_64 標準)
//     constexpr size_t CACHE_LINE_SIZE = 64;
// #endif

#define CACHE_LINE_SIZE 64

template <typename T, size_t Capacity>
class SPSCQueue {
private:
    // 1. 資料緩衝區 (連續記憶體)
    // T buffer_[Capacity];
    T* buffer_;

    // 2. 寫入位置 (由 Producer 修改)
    // alignas 確保它獨佔一條 Cache Line，不跟其他人打架
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_ = {0};

    // 3. 讀取位置 (由 Consumer 修改)
    // alignas 確保它獨佔另一條 Cache Line
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_ = {0};

public:
    // Writer 呼叫 (Producer)
    SPSCQueue() {
        buffer_ = new(std::nothrow) T[Capacity]; // 使用 nothrow 方便檢查
        if (!buffer_) {
            errorLog("SPSCQUEUE記憶體不足");
            // 處理記憶體不足的情況
        }
    }
    ~SPSCQueue() { delete[] buffer_; }
    bool push(const T& val) {
        size_t current_head = head_.load(std::memory_order_relaxed);
        size_t next_head = (current_head + 1) % Capacity;

        // 檢查是否滿了 (這裡要讀取 tail，這是唯一的競爭點)
        if (next_head == tail_.load(std::memory_order_acquire)) {
            errorLog("SPSCQueue push failed: Queue Full");
            return false; // Queue Full
        }

        // 寫入資料
        buffer_[current_head] = val;

        // 更新 head (Release: 保證上面的資料寫入先完成，再讓 Reader 看到 index 更新)
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    // Reader 呼叫 (Consumer)
    bool pop(T& val) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);

        // 檢查是否空了 (這裡要讀取 head)
        if (current_tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue Empty
        }

        // 讀取資料
        val = buffer_[current_tail];

        // 更新 tail (Release: 讓 Writer 知道這個位置可以重複使用了)
        size_t next_tail = (current_tail + 1) % Capacity;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    // Reader 專用：查看是否有資料 (Busy Spin 用)
    bool has_data() const {
        // Relaxed 即可，只是一個快速檢查
        return tail_.load(std::memory_order_relaxed) != head_.load(std::memory_order_relaxed);
    }

    void clear() {
        // 1. 讀取目前 Producer 寫到哪裡了 (Acquire 以確保看到最新的 head)
        size_t current_head = head_.load(std::memory_order_acquire);

        // 2. 直接把 tail 移動到 head 的位置
        // 意義上等於：「我瞬間已讀/丟棄了所有中間的資料」
        // Release 讓 Producer 知道這些空間釋放出來了
        tail_.store(current_head, std::memory_order_release);
    }
//---------------------------------------------------------------------------------------------------------------------------------------
    // 【新功能】取得下一個寫入位置的指標 (不移動 head)
    T* alloc() {
        size_t current_head = head_.load(std::memory_order_relaxed);
        size_t next_head = (current_head + 1) % Capacity;
        
        // 檢查是否滿了 (這裡可以用 cached_tail 優化)
        if (next_head == tail_.load(std::memory_order_acquire)) {
            errorLog("SPSCQueue push failed: Queue Full (alloc)");
            return nullptr; // 滿了
        }
        
        // 回傳 buffer 中該位置的指標，讓外面直接寫入
        return &buffer_[current_head];
    }

    // 【新功能】提交寫入 (移動 head)
    void push() {
        size_t current_head = head_.load(std::memory_order_relaxed);
        size_t next_head = (current_head + 1) % Capacity;
        
        // 這裡不需要再檢查滿不滿，因為 alloc 檢查過了
        // 直接發佈
        head_.store(next_head, std::memory_order_release);
    }


    T* front() {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        
        // 檢查是否有資料 (需 acquire 確保看到 head 的最新狀態)
        if (current_tail == head_.load(std::memory_order_acquire)) {
            return nullptr; // 空的
        }

        // 直接回傳 Buffer 裡的地址
        return &buffer_[current_tail];
    }

    // --- 【新增】Zero-Copy 讀取第二步：釋放 (Advance) ---
    // 呼叫這個函數代表你已經 "讀完/用完" front() 給你的資料了
    // 只有呼叫這個之後，Producer 才能覆蓋這一格
    void pop() {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) % Capacity;
        
        // 更新 tail，通知 Producer 這一格空出來了
        tail_.store(next_tail, std::memory_order_release);
    }
};