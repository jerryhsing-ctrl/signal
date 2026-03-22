#include <cstring>
#include <iostream>
#include <algorithm>

// N 是最大長度，包含結尾 \0
template<size_t N>
class FixedString {
private:
    char data_[N];
    size_t length_ = 0;

public:
    // 建構子
    FixedString() { data_[0] = '\0'; }
    
    FixedString(const char* s) {
        set(s);
    }

    // 賦值
    void set(const char* s) {
        // 使用 strncpy 或更快的 memcpy
        // 在 HFT 中我們通常確保 s 不會超過 N，或用更快的複製手段
        size_t i = 0;
        for (; i < N - 1 && s[i] != '\0'; ++i) {
            data_[i] = s[i];
        }
        data_[i] = '\0';
        length_ = i;
    }

    // 比較 (Hot Path 會用到)
    bool operator==(const FixedString& other) const {
        // 優化：先比長度，長度一樣再比內容
        if (length_ != other.length_) return false;
        return std::memcmp(data_, other.data_, length_) == 0;
    }
    
    bool operator==(const char* other) const {
        return std::strcmp(data_, other) == 0;
    }

    // 轉型
    const char* c_str() const { return data_; }
    size_t size() const { return length_; }
};