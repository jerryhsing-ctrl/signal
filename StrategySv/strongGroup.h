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
    long long group_vol_ratio_exempt_threshold;
    bool filter_prev_day_limit_up;
    bool exclude_prev_limit_up_from_rank;
    bool exclude_disposition_from_rank = true;
    bool member_cond1_enabled;
    bool member_cond2_enabled;
    bool member_cond4_enabled;
    double entry_min_vwap_pct_chg = 0.0;
    double entry_max_vwap_pct_chg = 0.0;  // 0 = no limit
    int entry_min_group_rank = 0;         // 0 = no limit; e.g. 2 = exclude G1
    bool require_raw_m1 = false;
    bool block_disposition_entry = true;  // block disposition stocks from entry
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


public:
    GroupRank groupRank;
    // GroupRank vwapRank;
    unordered_map<std::string, GroupRank> group_member_vwapRank;
    unordered_map<std::string, GroupRank> group_member_raw_vwapRank; // 僅過濾 member_min_month_trading_val，不做其他過濾
private:
    unordered_map<std::string, bool> symbol_is_valid;

public:
    // populated by on_tick when a stock qualifies — consumed by Order::trigger()
    struct MatchInfo {
        std::string group_name;
        int group_rank = 0;    // group's rank among all groups (1-based)
        int member_rank = 0;   // stock's rank within the group (1-based)
        int raw_member_rank = 0; // 僅過濾成交值，不做其他過濾的 VWAP 排名
        std::string m1_symbol;  // debug: M1 at the time last_match_info was set
        double vol_ratio = 0;           // 當日累計量 / 月均同時段量
        long long month_trading_val = 0; // 月均成交金額
    };
    unordered_map<std::string, MatchInfo> last_match_info;

    bool isSingleAllowed(const std::string& symbol, int maxRank);
    int getGroupLimitUpCount(const std::string& group);
};