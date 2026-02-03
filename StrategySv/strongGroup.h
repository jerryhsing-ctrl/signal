#pragma once
#include "topTracker.h"
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <string>
#include "type.h"
#include "indexCalc.h"
#include "Format1.h"
#include "QuoteSv.h"

class StrongGroup {
public:
    StrongGroup();
    void getData();

    bool on_tick(IndexData &idx, format6Type *f6);
    unordered_map<std::string, bool> is_strong_stock;

    void readFile(std::string);

    unordered_map<std::string, std::string> symbol_to_group;

    QuoteSv *quoteSv;
private:
    unordered_map<std::string, set<std::string>> group_member;
    unordered_map<std::string, long long> tradingValue_monthAvg;  // 過去 20天平均成交量
    unordered_map<std::string, long long> group_tradingValue_monthAvg_sum;  // 族群過去20天平均成交量加總
    
    unordered_map<std::string, long long> tradingValue_cumu;
    unordered_map<std::string, long long> group_tradingValue_cumu;
    unordered_map<std::string, long long> price_last;
    // unordered_map<std::string, double> percentageChg;
    // unordered_map<std::string, double> group_percentageChg_sum;
    unordered_map<std::string, long long> vol_cumu;

    double groupPercentageChg(std::string);
    double percentagChg(string symbol, long long price);

    bool isValidGroup(IndexData &idx, format6Type *f6);
};