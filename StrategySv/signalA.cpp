#include "signalA.h"
#include "IniReader.h"
#include <iostream>

signalA::signalA() {
    // Default values
    double vol_contract_ratio = 0.8;

    double track_zone_vwap_ratio = 1.003;
    double track_zone_day_high_ratio = 0.985;

    
    long long buffer_zone_duration_us = 240000000;
    double buffer_zone_exit_vwap_ratio = 1.005;

    // double buffer_zone_rolling_short_div = 2.5;
    // double buffer_zone_rolling_long_div = 10.0;
    double buffer_zone_day_high_ratio = 0.99;
    double buffer_zone_vwap_entry_ratio = 0.985;


    long long trade_zone_duration_us = 1200000000;
    double trade_zone_exit_price_ratio = 0.99;
    
    double trade_zone_vwap_ratio = 1.0057;

    IniReader reader;
    if (reader.ReadIni("./cfg/parameter.cfg")) {
        string section = "SignalA";
        try {
            string val;
            val = reader.Read(section.c_str(), "track_zone_vwap_ratio");
            if (!val.empty()) track_zone_vwap_ratio = stod(val);

            val = reader.Read(section.c_str(), "track_zone_day_high_ratio");
            if (!val.empty()) track_zone_vwap_ratio = stod(val);

            val = reader.Read(section.c_str(), "buffer_zone_duration_min");
            if (!val.empty()) buffer_zone_duration_us = stoll(val) * 60 * 1000 * 1000;

            val = reader.Read(section.c_str(), "buffer_zone_exit_vwap_ratio");
            if (!val.empty()) buffer_zone_exit_vwap_ratio = stod(val);


            val = reader.Read(section.c_str(), "vol_contract_ratio");
            if (!val.empty()) vol_contract_ratio = stod(val);

            val = reader.Read(section.c_str(), "buffer_zone_day_high_ratio");
            if (!val.empty()) buffer_zone_day_high_ratio = stod(val);

            val = reader.Read(section.c_str(), "buffer_zone_vwap_entry_ratio");
            if (!val.empty()) buffer_zone_vwap_entry_ratio = stod(val);

            val = reader.Read(section.c_str(), "trade_zone_vwap_ratio");
            if (!val.empty()) trade_zone_vwap_ratio = stod(val);

            val = reader.Read(section.c_str(), "trade_zone_exit_price_ratio");
            if (!val.empty()) trade_zone_exit_price_ratio = stod(val);

            val = reader.Read(section.c_str(), "trade_zone_duration_min");
            if (!val.empty()) trade_zone_duration_us = stoll(val) * 60 * 1000 * 1000;
        } catch (const std::exception& e) {
            std::cerr << "Error parsing config: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Failed to read ./cfg/parameter.cfg" << std::endl;
    }

    config.set(
        vol_contract_ratio,
        track_zone_vwap_ratio,
        track_zone_day_high_ratio,
        buffer_zone_duration_us,
        buffer_zone_exit_vwap_ratio,
        buffer_zone_day_high_ratio,
        buffer_zone_vwap_entry_ratio,
        trade_zone_duration_us,
        trade_zone_exit_price_ratio,
        trade_zone_vwap_ratio
    );
}

bool signalA::eval(IndexData &idx, format6Type *f6, bool choosen) {
    if (symbol.empty())
        symbol = f6->symbol;

    // cout << "signalA\n";

    if (!pre_condition_met(idx, f6)) 
        return false;

    bool trigger = false;
    if (choosen) {
        if (track_zone_eval(idx, f6)) {
            in_buffer_zone = true;
            buffer_zone_trigger_price = f6->match.Price;
            buffer_zone_start_time = f6->matchTime_us;
        }
    }

    if (in_buffer_zone) {
        if (buffer_zone_exit(idx, f6)) {
            in_buffer_zone = false;
            buffer_zone_trigger_price = -1;
            buffer_zone_start_time = -1;
        } 
        else if (buffer_zone_eval(idx, f6)) {
            in_trade_zone = true;
            trade_zone_trigger_price = buffer_zone_trigger_price;
            trade_zone_start_time = f6->matchTime_us;
        }
    }

    if (in_trade_zone) {
        if (trade_zone_exit(idx, f6)) {
            in_trade_zone = false;
            trade_zone_trigger_price = -1;
            trade_zone_start_time = -1;
        } 
        else if (trade_zone_eval(idx, f6)) {
            trigger = true;
            enter_market = true;
        }
    }



    return trigger;
}

bool signalA::pre_condition_met(IndexData &idx, format6Type *f6) {
    return true;
}


bool signalA::track_zone_eval(IndexData &idx, format6Type *f6) {
    bool cond1 = f6->match.Price <= idx.vwap * config.track_zone_vwap_ratio && f6->match.Price <= config.track_zone_day_high_ratio * idx.day_high;
    bool cond2 = f6->match.Price <= idx.rolling_low;

    return cond1 && cond2;
}

bool signalA::buffer_zone_exit(IndexData &idx, format6Type *f6) {
    bool cond1 = f6->matchTime_us - buffer_zone_start_time > config.buffer_zone_duration_us;
    bool cond2 = f6->match.Price >= idx.vwap * config.buffer_zone_exit_vwap_ratio;

    return cond1 || cond2;
}

bool signalA::buffer_zone_eval(IndexData &idx, format6Type *f6) {
    // cout << idx.rolling_sum_ratio << " " << config.vol_contract_ratio << '\n';
    bool cond1 = ((double) idx.rolling_sum_ratio ) < config.vol_contract_ratio;
    bool cond2 = f6->match.Price <= idx.day_high * config.buffer_zone_day_high_ratio;
    bool cond3 = f6->match.Price >= idx.vwap * config.buffer_zone_vwap_entry_ratio;
    
    return cond1 && cond2 && cond3;
}

bool signalA::trade_zone_exit(IndexData &idx, format6Type *f6) {
    //取消條件: Price <= trigger_price * 0.99
    bool cond1 = f6->matchTime_us - trade_zone_start_time > config.trade_zone_duration_us;
    bool cond2 = f6->match.Price <= trade_zone_trigger_price * config.trade_zone_exit_price_ratio;

    return cond1 || cond2;
}

bool signalA::trade_zone_eval(IndexData &idx, format6Type *f6) {
    

    return f6->match.Price >= idx.vwap * config.trade_zone_vwap_ratio;
}



bool signalA::leave(IndexData &idx, format6Type *f6) {
    // if (enter_market) {
    //     price <= idx.vwap * 0.997;
    // }
    if (!true)
        enter_market = false;
    return false;
}
