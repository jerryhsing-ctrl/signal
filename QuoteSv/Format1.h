#pragma once
#include <string>
#include <unordered_map>
#include <string>

#pragma once
class Format1 {
public:
    std::string symbol;
    std::string name;
    std::string market; 
    
    double previous_close;
    double limit_up_price;
    double limit_down_price;

    std::string industry;
    std::string security;
    std::string errorCode;
    
    void print() const;
};

class Format1Manager {
public:
    bool readFile(const std::string &date);
    const Format1 &getF1(const std::string &fsymbol) const;
    void printAll() const;

    std::unordered_map<std::string, Format1> format1Map;

};