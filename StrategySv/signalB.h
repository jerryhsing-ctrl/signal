#pragma once
#include "indexCalc.h"
#include "Format6.h"
#include "type.h"
#include "tick.h"

struct SignalBConfig {
    double vol_contract_ratio;
    double pre_condition_vwap_ratio;
    double track_zone_vwap_ratio;
    long long buffer_zone_duration_us;
    double buffer_zone_exit_price_ratio;
    double buffer_zone_rolling_short_div;
    double buffer_zone_rolling_long_div;
    double buffer_zone_day_high_ratio;
    long long trade_zone_duration_us;
    double trade_zone_eval_price_ratio;
    int trade_zone_eval_tick_add;
    int trade_zone_exit_tick_sub;

    void set(
        double vol_contract_ratio,
        double pre_condition_vwap_ratio,
        double track_zone_vwap_ratio,
        long long buffer_zone_duration_us,
        double buffer_zone_exit_price_ratio,
        double buffer_zone_rolling_short_div,
        double buffer_zone_rolling_long_div,
        double buffer_zone_day_high_ratio,
        long long trade_zone_duration_us,
        double trade_zone_eval_price_ratio,
        int trade_zone_eval_tick_add,
        int trade_zone_exit_tick_sub
    ) {
        this->vol_contract_ratio = vol_contract_ratio;
        this->pre_condition_vwap_ratio = pre_condition_vwap_ratio;
        this->track_zone_vwap_ratio = track_zone_vwap_ratio;
        this->buffer_zone_duration_us = buffer_zone_duration_us;
        this->buffer_zone_exit_price_ratio = buffer_zone_exit_price_ratio;
        this->buffer_zone_rolling_short_div = buffer_zone_rolling_short_div;
        this->buffer_zone_rolling_long_div = buffer_zone_rolling_long_div;
        this->buffer_zone_day_high_ratio = buffer_zone_day_high_ratio;
        this->trade_zone_duration_us = trade_zone_duration_us;
        this->trade_zone_eval_price_ratio = trade_zone_eval_price_ratio;
        this->trade_zone_eval_tick_add = trade_zone_eval_tick_add;
        this->trade_zone_exit_tick_sub = trade_zone_exit_tick_sub;
    }
};

class signalB {
public:
    signalB();
    bool eval(IndexData &idx, format6Type *f6);
    bool leave(IndexData &idx, format6Type *f6);

private:
    string symbol = "";
    bool forbidden = false;

    SignalBConfig config;

    
    bool pre_condition_met(IndexData &idx, format6Type *f6);
    

    bool track_zone_eval(IndexData &idx, format6Type *f6);


    bool in_buffer_zone = false;
    long long buffer_zone_trigger_price = -1;
    long long buffer_zone_start_time = -1;
    bool buffer_zone_eval(IndexData &idx, format6Type *f6);
    bool buffer_zone_exit(IndexData &idx, format6Type *f6);
    
    bool in_trade_zone = false;
    long long trade_zone_trigger_price = -1;
    long long trade_zone_start_time = -1;
    bool trade_zone_eval(IndexData &idx, format6Type *f6);
    bool trade_zone_exit(IndexData &idx, format6Type *f6);

    bool enter_market = false;
};