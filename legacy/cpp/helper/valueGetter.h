#pragma once
#include "IniReader.h"
#include "errorLog.h"



inline double getDoubleValue(const std::string& section, const std::string& key) {
    static IniReader reader(cfgFile);
    std::string valueStr = reader.Read(section.c_str(), key.c_str());
    if (valueStr.empty()) {
        errorLog("Key not found: " + section + " -> " + key);
    }
    for (char c : valueStr) {
        if (c < '0' || c > '9') {
            errorLog("Invalid double format: [" + valueStr + "] in " + section + " -> " + key);
        }
    }
    
    return std::stod(valueStr);
}