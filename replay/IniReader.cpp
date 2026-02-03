//---------------------------------------------------------------------------
#include "IniReader.h"
#include <stdio.h>
#include <string.h>
#include "Utilities.h"
//---------------------------------------------------------------------------
IniReader::IniReader() : Ini_(nullptr) {
    Filename_.clear();
}

IniReader::IniReader(const char* fname) : Ini_(nullptr) {
    ReadIni(fname);
}

IniReader::~IniReader() {
    if (Ini_) {
        fclose(Ini_);
        Ini_ = nullptr;
    }
}

//---------------------------------------------------------------------------
// 基本操作
//---------------------------------------------------------------------------
void IniReader::ResetFile(const char* fname) {
    IniMap_.clear();
    Filename_.assign(fname);
    if (Ini_) {
        fclose(Ini_);
        Ini_ = nullptr;
    }
}

//---------------------------------------------------------------------------
// 讀取 ini
//---------------------------------------------------------------------------
bool IniReader::ReadIni(const char* fname) {
    ResetFile(fname);

    Ini_ = fopen(fname, "r");
    if (!Ini_) return false;

    std::string key1, key2, val, temp;
    while (true) {
        char buf[1024] = {0};
        if (!fgets(buf, sizeof(buf), Ini_)) break;
        temp.assign(buf);

        auto pL  = temp.find('[');
        auto pEq = temp.find('=');

        if (pL != std::string::npos) {
            auto pR = temp.find(']', pL+1);
            if (pR != std::string::npos)
                key1 = temp.substr(pL+1, pR - (pL+1));
        } else if (pEq != std::string::npos) {
            key2 = temp.substr(0, pEq);
            val  = temp.substr(pEq+1);

            // 去掉換行符
            while (!val.empty() && (val.back() == '\n' || val.back() == '\r'))
                val.pop_back();

            updateMap2D<TIniDetailMap,TIniListMap,TIniKey>(IniMap_, key1, key2, val);
        }
    }

    fclose(Ini_);
    Ini_ = nullptr;
    return true;
}

//---------------------------------------------------------------------------
// 寫入 ini
//---------------------------------------------------------------------------
bool IniReader::WriteIni(const char* fname) {
    if (IniMap_.empty()) return true;
    if (fname) Filename_.assign(fname);

    if (Ini_) {
        fclose(Ini_);
        Ini_ = nullptr;
    }
    Ini_ = fopen(Filename_.c_str(), "w");
    if (!Ini_) return false;

    char buf[1024];
    const std::string empty;

    auto it0 = IniMap_.find(empty);
    if (it0 != IniMap_.end()) {
        for (auto& kv : it0->second) {
            snprintf(buf, sizeof(buf), "%s=%s", kv.first.c_str(), kv.second.c_str());
            fputs(buf, Ini_);
            if (buf[strlen(buf)-1] != '\n') fputs("\n", Ini_);
        }
    }

    for (auto& sec : IniMap_) {
        if (sec.first == empty) continue;
        snprintf(buf, sizeof(buf), "[%s]", sec.first.c_str());
        fputs(buf, Ini_);
        if (buf[strlen(buf)-1] != '\n') fputs("\n", Ini_);

        for (auto& kv : sec.second) {
            snprintf(buf, sizeof(buf), "%s=%s", kv.first.c_str(), kv.second.c_str());
            fputs(buf, Ini_);
            if (buf[strlen(buf)-1] != '\n') fputs("\n", Ini_);
        }
    }

    fclose(Ini_);
    Ini_ = nullptr;
    return true;
}

std::string IniReader::Read(const char* key1, const char* key2)
{
    const char* s1 = key1 ? key1 : "";
    const char* s2 = key2 ? key2 : "";

    auto it1 = IniMap_.find(s1);
    if (it1 != IniMap_.end()) {
        auto it2 = it1->second.find(s2);
        if (it2 != it1->second.end())
            return it2->second;
    }
    return std::string();
}

std::string IniReader::Read(const char* key)
{
    const std::string empty;
    const char* k = key ? key : "";

    auto it1 = IniMap_.find(empty);
    if (it1 != IniMap_.end()) {
        auto it2 = it1->second.find(k);
        if (it2 != it1->second.end())
            return it2->second;
    }
    return std::string();
}
