/* 
 * File:   IniReader.h
 *
 */

#pragma once
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <cctype>

//---------------------------------------------------------------------------
template<class MapA,class MapB,class Valtype>
void updateMap2D(MapA& MapA_,typename MapA::key_type key1,typename MapB::key_type key2,Valtype val)
{
	typedef std::pair<typename MapB::key_type,Valtype> TListPair;
	typedef std::pair<typename MapA::key_type,MapB> TDetailPair;
	typename MapA::iterator iter = MapA_.find(key1);
	if (iter == MapA_.end()) {
		MapB Listmap_;
		Listmap_.insert(TListPair(key2,val));
		MapA_.insert(TDetailPair(key1,Listmap_));
	}
	else {
		typename MapB::iterator it = iter->second.find(key2);
		if (it == iter->second.end())
			iter->second.insert(TListPair(key2,val));
		else
			it->second = val;
	}
}
//---------------------------------------------------------------------------
// 移除字串前後的空白/換行/Tab
inline void trim(std::string& s) {
    // 去掉前面
    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(),
                         [](unsigned char ch) { return !std::isspace(ch); }));

    // 去掉後面
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char ch) { return !std::isspace(ch); }).base(),
            s.end());
}
//---------------------------------------------------------------------------
// 不改變原字串，回傳一個新的
inline std::string trim_copy(std::string s) {
    trim(s);
    return s;
}

//---------------------------------------------------------------------------
#include <string>
#include <map>
//---------------------------------------------------------------------------
typedef std::string TIniKey;
typedef std::map<TIniKey,TIniKey> 	  TIniListMap;		//供xxx=xxx的ini
typedef std::map<TIniKey,TIniListMap> TIniDetailMap;	//建立[Type]-[xxx=xxx]的Map
//---------------------------------------------------------------------------

class IniReader {
public:
    IniReader();
    explicit IniReader(const char* fname);
    ~IniReader();

    void ResetFile(const char* fname);
    bool ReadIni(const char* fname);
    bool WriteIni(const char* fname = nullptr);

    std::string Read(const char* key1, const char* key2);
    std::string Read(const char* key);
    void Write(const char* key1, const char* key2, const char* val);
    void Write(const char* key, const char* val);
    void ReadSector(const char* key, std::map<std::string,std::string>& map);

private:
    std::string Filename_;
    FILE* Ini_;   // 檔案指標
    // 這裡放你的 IniMap_ 以及相關 typedef
    TIniDetailMap IniMap_;
};

