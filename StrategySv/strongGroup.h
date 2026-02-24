#pragma once
#include "topTracker.h"
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vector>
#include <string>
#include "type.h"
#include "indexCalc.h"
#include "Format1.h"
#include "QuoteSv.h"
#include "rank.h"

struct StrongGroupConfig {
    long long member_min_month_trading_val;
    double member_strong_vol_ratio;
    long long member_strong_trading_val;
    int top_group_rank_threshold;
    int top_group_max_select;
    int top_group_min_select;
    int normal_group_max_select;
    int normal_group_min_select;
    long long group_min_month_trading_val;
    double group_min_avg_pct_chg;
    double group_min_val_ratio;
    double member_vwap_pct_chg_threshold;
    int group_valid_top_n;
    bool is_weighted_avg;
};

class StrongGroup {
public:
    StrongGroup();
    void getGroup();

    bool on_tick(IndexData &idx, format6Type *f6);
    unordered_map<std::string, bool> is_strong_stock;

    void readFile(std::string);

    unordered_map<std::string, vector<std::string>> symbol_to_groups;

    QuoteSv *quoteSv;
private:
    StrongGroupConfig config;
    unordered_map<std::string, set<std::string>> group_member;
    unordered_map<std::string, long long> tradingValue_monthAvg;  // 過去 20天平均成交量
    unordered_map<std::string, long long> group_tradingValue_monthAvg_sum;  // 族群過去20天平均成交量加總
    
    unordered_map<std::string, long long> tradingValue_cumu;
    unordered_map<std::string, long long> group_tradingValue_cumu;
    unordered_map<std::string, long long> price_last;
    // unordered_map<std::string, double> percentageChg;
    // unordered_map<std::string, double> group_percentageChg_sum;
    unordered_map<std::string, long long> vol_cumu;

    unordered_map<std::string, int> group_member_count;
    double groupPercentageChg(std::string, bool weighted_avg);
    double percentagChg(string symbol, long long price);

    bool isValidGroup(IndexData &idx, format6Type *f6, const std::string& group);


    GroupRank groupRank;
    // GroupRank vwapRank;
    unordered_map<std::string, GroupRank> group_member_vwapRank;
    unordered_map<std::string, bool> symbol_is_valid;
};