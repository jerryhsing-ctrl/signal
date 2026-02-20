#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
using namespace std;
#include <string>

extern "C" {

    // 加密：明文 → Base64(IV + Cipher)
    const char* Encrypt(const char* plainText);

    // 解密：Base64 → 明文
    const char* Decrypt(const char* base64Cipher);

    // 釋放回傳字串記憶體
    void FreeString(const char* p);
}
inline std::string GetTodayString() {
    time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    char datebuf[16];
    strftime(datebuf, sizeof(datebuf), "%Y%m%d", &tm);
    return std::string(datebuf);
}

inline vector<vector<string>> DecInventory(const std::string &path) {
    vector<vector<string>> Inventory;

    std::ifstream fin(path);
    std::string outname = std::string("./log/Inventory_") + GetTodayString() + ".txt";
    std::ofstream fout(outname, std::ios::trunc);

    // 移除開頭3個多餘字元: -17 -69 -65 (BOM)
    auto RemoveBOM = [](std::string& s) {
        if (s.size() >= 3 && 
            (unsigned char)s[0] == 0xEF && 
            (unsigned char)s[1] == 0xBB && 
            (unsigned char)s[2] == 0xBF) {
            s.erase(0, 3); // 移除前三個位元組
        }
    };

    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty()) continue;
        const char *decryptedChar = Decrypt(line.c_str());
        if (decryptedChar == nullptr) {
            continue;
        }
        std::string content(decryptedChar);
        RemoveBOM(content);
        std::stringstream ss(content);
        std::string cell;
        std::vector<std::string> cols;
        while (std::getline(ss, cell, ',')) {
            cols.push_back(cell);
        }
        FreeString(decryptedChar);

        // 印出本行所有欄位（逗號分隔）
        // for (size_t i = 0; i < cols.size(); ++i) {
        //     std::cout << cols[i];
        //     if (i + 1 < cols.size()) std::cout << ",";
        // }
        // std::cout << std::endl;
        // 寫入檔案
        for (size_t i = 0; i < cols.size(); ++i) {
            fout << cols[i];
            if (i + 1 < cols.size()) fout << ",";
        }
        fout << std::endl;
        Inventory.push_back(cols);
    }
    fout.close();
    return Inventory;
}