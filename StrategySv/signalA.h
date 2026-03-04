#pragma once
#include "indexCalc.h"
#include "Format6.h"
#include "type.h"

class StrongSingle;
class StrongGroup;
class QuoteSv;

struct SignalAConfig {
    double vwap_touch_ratio = 0.002;
    long long entry_start_time = 92000000000;
    long long entry_end_time = 110000000000;
    long long pre_condition_start_time = 91500000000;
    double pre_condition_vwap_ratio = 0.993;
    double trade_zone_max_increase_ratio = 0.085;
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
    QuoteSv *quoteSv = nullptr;

    SignalAConfig config;

    bool pre_condition_met(IndexData &idx, format6Type *f6);
};
