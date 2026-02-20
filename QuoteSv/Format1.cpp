#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <algorithm> // <-- 需要此標頭檔
#include <cctype>    // <-- 需要此標頭檔
#include <unistd.h>
#include "Format1.h"
#include "errorLog.h"
#include <filesystem>


// ==========================================================
// 輔助函數：去除字串前後的空白
// (static inline 表示此函數僅在此 .cpp 檔案內部使用)
// ==========================================================
static inline void trim(std::string& s) {
    // 從右邊 (尾端) 去除空白
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    
    // 從左邊 (開頭) 去除空白
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}


/**
 * @brief 實作 readFile (已更新：自動去除所有欄位的前後空白)
 */
bool Format1Manager::readFile(const std::string& date) {
    std::string filename = "./files/Symbols_" + date + ".csv";
    std::ifstream file(filename);
    if (!file.is_open()) {
        // std::cerr << "Error: Could not open file " << filename << std::endl;
        errorLog("Error: Could not open file " + filename);
        return false;
    }

    std::string line;

    // 讀取並跳過標頭
    // if (!std::getline(file, line)) {
    //     std::cerr << "Error: File is empty or cannot be read." << std::endl;
    //     return false;
    // }

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line);
        std::string segment;
        std::string symbol; // 現在 symbol 會是乾淨的
        Format1 data;
        
        std::string name2;

      try {
            // (1) symbol
            if (!std::getline(ss, symbol, ',')) { continue; }
            trim(symbol);
            data.symbol = symbol;

            // (2) name
            if (!std::getline(ss, segment, ',')) { continue; }
            trim(segment);
            data.name = segment;
            name2 = segment;

            // (3) market
            if (!std::getline(ss, segment, ',')) { continue; }
            trim(segment);
            data.market = segment;

            // (4) previous_close
            if (!std::getline(ss, segment, ',')) { continue; }
            trim(segment);
            data.previous_close = std::stod(segment);

            // (5) limit_up_price
            if (!std::getline(ss, segment, ',')) { continue; }
            trim(segment);
            data.limit_up_price = std::stod(segment);

            // (6) limit_down_price
            if (!std::getline(ss, segment, ',')) { continue; }
            trim(segment);
            data.limit_down_price = std::stod(segment);

            // (7) industry
            if (!std::getline(ss, segment, ',')) { continue; }
            trim(segment);
            data.industry = segment;

            // (8) security
            if (!std::getline(ss, segment, ',')) { continue; }
            trim(segment);
            data.security = segment;

            // (9) errorCode
            if (!std::getline(ss, segment, ',')) { continue; }
            trim(segment);
            data.errorCode = segment;

        
            if (symbol.empty()) {
                continue;
            }

            // format1Map.emplace(stoi(symbol), data);
            format1Map.emplace(symbol, data);
        } catch (const std::exception& e) {
            // (stod 轉換失敗，例如欄位 trim 後是空字串 "")
            std::cerr << "Warning: Skipping malformed line: " << line << " (" << e.what() << ")" << std::endl;
        }

    }

    file.close();
    return true;
}

/**
 * @brief 實作 getData (此函數不需修改)
 */
const Format1& Format1Manager::getF1(const std::string &symbol) const {
    // auto it = format1Map.find(stoi(symbol));
    auto it = format1Map.find(symbol);

    if (it != format1Map.end()) {
        return it->second;
    } else {
        static Format1 emptyFormat1;
        emptyFormat1.previous_close = -1;

        errorLog("Warning: Symbol [" + symbol + "] not found in Format1Manager.");
        return emptyFormat1;
    }

}

void Format1Manager::printAll() const {
    for (const auto& pair : format1Map) {
        pair.second.print();
    }
}

void Format1::print() const {
    std::cout << "Symbol: " << symbol
              << ", Name: " << name
              << ", Market: " << market
              << ", Previous Close: " << previous_close
              << ", Limit Up Price: " << limit_up_price
              << ", Limit Down Price: " << limit_down_price
              << ", Industry: " << industry
              << ", Security: " << security
              << ", Error Code: " << errorCode
              << std::endl;
}