/* 
 * File:   IniReader.h
 *
 */

#ifndef INIREADER_H
#define INIREADER_H
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
#endif /* IniReader_H */

