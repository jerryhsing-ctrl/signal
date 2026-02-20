#pragma once
#include <vector>
#include <cstddef> // for std::byte
#include <utility> // for std::forward
#include <cassert>

template <typename T>
class FixedVector {
private:
    // 使用 byte 陣列作為底層儲存 (只佔空間，不建構物件)
    // alignas 確保 T 的記憶體對齊正確 (避免 Crash 或效能低落)
    alignas(T) std::vector<std::byte> storage_;
    size_t size_ = 0;
    size_t capacity_ = 0;

public:
    // 初始化時必須決定最大容量 (因為我們承諾不搬家)
    explicit FixedVector(size_t cap) : capacity_(cap) {
        storage_.resize(cap * sizeof(T));
    }

    // 解構時，必須手動清理所有已建構的物件
    ~FixedVector() {
        clear();
    }

    // 【核心】原地建構 (emplace_back 的手動版)
    template <typename... Args>
    T* emplace_back(Args&&... args) {
        if (size_ >= capacity_) {
            // HFT 通常在這裡報錯或 throw，絕對不能 resize
            return nullptr; 
        }

        // 1. 算出第 size_ 個物件的記憶體地址
        T* ptr = reinterpret_cast<T*>(&storage_[size_ * sizeof(T)]);

        // 2. Placement New: 在這個地址上呼叫建構子
        // 這會繞過所有 Move/Copy 的檢查
        new (ptr) T(std::forward<Args>(args)...);

        size_++;
        return ptr;
    }

    // 陣列存取運算子 (跟 vector 一樣用法)
    T& operator[](size_t index) {
        return *reinterpret_cast<T*>(&storage_[index * sizeof(T)]);
    }

    const T& operator[](size_t index) const {
        return *reinterpret_cast<const T*>(&storage_[index * sizeof(T)]);
    }

    void clear() {
        // 反向解構 (C++ 標準順序)
        for (size_t i = size_; i > 0; --i) {
            T* ptr = reinterpret_cast<T*>(&storage_[(i - 1) * sizeof(T)]);
            ptr->~T(); // 手動呼叫解構子
        }
        size_ = 0;
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
};