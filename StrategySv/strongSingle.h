#pragma once
#include "topTracker.h"
#include <unordered_map>
#include "type.h"
#include "indexCalc.h"
#include "Format1.h"
#include "QuoteSv.h"

struct StrongSingleConfig {
    int monitor_pool_size;
    long long min_month_trading_val;
    double price_amplitude_threshold;
    double day_high_increase_threshold;
    double vol_increase_month_ratio;
    double vol_increase_yesterday_ratio;
    long long strong_month_trading_val;
    long long vwap_floor_start_time;
    double vwap_floor_ratio;
    double extreme_price_increase_limit;
};

class StrongSingle {
public:
    StrongSingle();
    TopKVolumeTracker topTracker;

    bool on_tick(IndexData &idx, format6Type *f6);
    unordered_map<std::string, bool> is_strong_stock;


    unordered_map<std::string, bool> symbol_is_valid;
    Format1Manager *f1mgr;
    QuoteSv *quoteSv;
    unordered_map<std::string, bool> forbidden;
private:
    StrongSingleConfig config;
    // double maxPriceAmp = 0;
    unordered_map<std::string, double> maxPriceAmp;
    bool eval_price_cond(IndexData &idx, format6Type *f6);
    
    unordered_map<std::string, long long> vol_cumu;
    bool eval_vol_cond(IndexData &idx, format6Type *f6);
    // bool forbidden = false;
    
    bool eval_vwap_cond(IndexData &idx, format6Type *f6);
    bool eval_extremeFilter_cond(IndexData &idx, format6Type *f6);
};