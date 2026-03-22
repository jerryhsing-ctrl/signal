//---------------------------------------------------------------------------
#pragma once
#include <cstdint>

inline char* PackBCD2Str(unsigned char *src, int index, int count) {
    // 每個 BCD byte 轉成兩個字元，加一個 '\0' 結尾
    char* rtnstr = new char[count * 2 + 1];

    for (int i = 0; i < count; ++i) {
        unsigned char byte = src[index + i];
        rtnstr[i * 2]     = ((byte >> 4) & 0x0F) + '0';
        rtnstr[i * 2 + 1] = (byte & 0x0F) + '0';
    }

    rtnstr[count * 2] = '\0'; // 字串結尾
    return rtnstr;
}

inline int BCDToIntegerFast(unsigned char val) {
    return val - (6 * (val >> 4));
}
// 編譯器會將其轉化為一連串的計算指令，中間無跳轉 (No Branching)
// 【終極優化】專門處理 2 Bytes (用於 message len) - 無迴圈
inline int PackBCD2(const unsigned char* __restrict p) {
    int res = BCDToIntegerFast(p[0]);
    res = res * 100 + BCDToIntegerFast(p[1]);
    return res;
}
// 【終極優化】專門處理 4 Bytes (用於 Qty) - 無迴圈
inline int PackBCD4(const unsigned char* __restrict p) {
    int res = BCDToIntegerFast(p[0]);
    res = res * 100 + BCDToIntegerFast(p[1]);
    res = res * 100 + BCDToIntegerFast(p[2]);
    res = res * 100 + BCDToIntegerFast(p[3]);
    return res;
}

// 【終極優化】專門處理 5 Bytes (用於 Price) - 無迴圈
inline int PackBCD5(const unsigned char* __restrict p) {
    int res = BCDToIntegerFast(p[0]);
    res = res * 100 + BCDToIntegerFast(p[1]);
    res = res * 100 + BCDToIntegerFast(p[2]);
    res = res * 100 + BCDToIntegerFast(p[3]);
    res = res * 100 + BCDToIntegerFast(p[4]);
    return res;
}

inline long long PackBCD6(const unsigned char* __restrict p) {
    long long res = BCDToIntegerFast(p[0]);
    res = res * 100 + BCDToIntegerFast(p[1]);
    res = res * 100 + BCDToIntegerFast(p[2]);
    res = res * 100 + BCDToIntegerFast(p[3]);
    res = res * 100 + BCDToIntegerFast(p[4]);
    res = res * 100 + BCDToIntegerFast(p[5]);
    return res;
}

// 【終極優化】專門處理 8 Bytes (用於 TotalMatchQty) - 無迴圈
// 回傳 long (因為 8 bytes BCD 可能超過 int 範圍)
inline long long PackBCD8(const unsigned char* __restrict p) {
    long long res = BCDToIntegerFast(p[0]);
    res = res * 100 + BCDToIntegerFast(p[1]);
    res = res * 100 + BCDToIntegerFast(p[2]);
    res = res * 100 + BCDToIntegerFast(p[3]);
    res = res * 100 + BCDToIntegerFast(p[4]);
    res = res * 100 + BCDToIntegerFast(p[5]);
    res = res * 100 + BCDToIntegerFast(p[6]);
    res = res * 100 + BCDToIntegerFast(p[7]);
    return res;
}
// //---------------------------------------------------------------------------------------------------------------------------------------------
// inline int BCDToIntegerFast(unsigned char val) {
//     return val - (6 * (val >> 4));
// }


// inline int PackBCD2Int(const unsigned char* __restrict src, int count) {
//     const unsigned char* ptr = src;
//     int result = 0;
    
//     // 如果 count 是編譯期常數，建議用 template loop unrolling，否則以下迴圈已極快
//     // 透過指標算術取代陣列索引 (src[index+i]) 以減少 CPU 指令
//     for (int i = 0; i < count; ++i) {
//         // result * 100 因為 BCD 一個 byte 代表兩位數
//         // 編譯器會將 result * 100 優化為 ((result << 6) + (result << 5) + (result << 2)) 或 IMUL
//         result = result * 100 + BCDToIntegerFast(*ptr++);
//     }
//     return result;
// }

// // -------------------------------------------------------------------------
// // 2. PackBCD2Long (回傳 long)
// // -------------------------------------------------------------------------
// inline long PackBCD2Long(const unsigned char* __restrict src, int count) {
//     const unsigned char* ptr = src;
//     long result = 0;
    
//     for (int i = 0; i < count; ++i) {
//         result = result * 100 + BCDToIntegerFast(*ptr++);
//     }
//     return result;
// }

// // -------------------------------------------------------------------------
// // 3. PackBCD2LongLong (回傳 long long)
// // -------------------------------------------------------------------------
// inline long long PackBCD2LongLong(const unsigned char* __restrict src, int count) {
//     const unsigned char* ptr = src;
//     long long result = 0;
    
//     for (int i = 0; i < count; ++i) {
//         result = result * 100 + BCDToIntegerFast(*ptr++);
//     }
//     return result;
// }



// // -------------------------------------------------------------------------
// // 1. PackBCD2Int (回傳 int)
// // -------------------------------------------------------------------------
// inline int PackBCD2Int(const unsigned char* __restrict src, int index, int count) {
//     const unsigned char* ptr = src + index;
//     int result = 0;
    
//     // 如果 count 是編譯期常數，建議用 template loop unrolling，否則以下迴圈已極快
//     // 透過指標算術取代陣列索引 (src[index+i]) 以減少 CPU 指令
//     for (int i = 0; i < count; ++i) {
//         // result * 100 因為 BCD 一個 byte 代表兩位數
//         // 編譯器會將 result * 100 優化為 ((result << 6) + (result << 5) + (result << 2)) 或 IMUL
//         result = result * 100 + BCDToIntegerFast(*ptr++);
//     }
//     return result;
// }

// // -------------------------------------------------------------------------
// // 2. PackBCD2Long (回傳 long)
// // -------------------------------------------------------------------------
// inline long PackBCD2Long(const unsigned char* __restrict src, int index, int count) {
//     const unsigned char* ptr = src + index;
//     long result = 0;
    
//     for (int i = 0; i < count; ++i) {
//         result = result * 100 + BCDToIntegerFast(*ptr++);
//     }
//     return result;
// }

// // -------------------------------------------------------------------------
// // 3. PackBCD2LongLong (回傳 long long)
// // -------------------------------------------------------------------------
// inline long long PackBCD2LongLong(const unsigned char* __restrict src, int index, int count) {
//     const unsigned char* ptr = src + index;
//     long long result = 0;
    
//     for (int i = 0; i < count; ++i) {
//         result = result * 100 + BCDToIntegerFast(*ptr++);
//     }
//     return result;
// }

// -------------------------------------------------------------------------
// 4. 單一 Byte 版本 (保留你的介面，但在內部優化)
// -------------------------------------------------------------------------
// inline int PackBCD2Int(unsigned char byt1) {
//     return BCDToIntegerFast(byt1);
// }