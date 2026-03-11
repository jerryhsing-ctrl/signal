#pragma once
#include "indexCalc.h"
#include "Format6.h"
#include "type.h"

class StrongSingle;
class StrongGroup;
class QuoteSv;

struct SignalAConfig {
    double vwap_near_ratio = 1.005;     // price/vwap <= this = "near VWAP"
    double bounce_ratio = 0.006;        // bounce 0.6% from low = entry
    long long entry_start_time = 92000000000;
    long long entry_end_time = 110000000000;
    long long pre_condition_start_time = 91500000000;
    double pre_condition_vwap_ratio = 0.993;
    double trade_zone_max_increase_ratio = 0.085;
    long long max_near_to_entry_us = 0;  // 0 = no limit; in microseconds
};

class signalA {
public:
    signalA();
    bool eval(IndexData &idx, format6Type *f6, MatchType matchType, MatchType &triggerMatchType, StrongSingle &strongSingle, StrongGroup &strongGroup, QuoteSv *quoteSv);
    bool leave(IndexData &idx, format6Type *f6);

private:
    string symbol = "";
    bool forbidden = false;
    bool triggered = false;
    bool near_vwap = false;
    long long low_since_near = 0;  // lowest price after approaching VWAP
    long long near_vwap_time_us = 0; // matchTime_us when near_vwap triggered
public:
    long long near_vwap_time = 0;  // matchTimeStr when near_vwap triggered (for tick_dump)
    double near_vwap_pv_ratio = 0; // price/vwap when near_vwap triggered (for tick_dump)
private:
    QuoteSv *quoteSv = nullptr;

    SignalAConfig config;

    bool pre_condition_met(IndexData &idx, format6Type *f6);
};
