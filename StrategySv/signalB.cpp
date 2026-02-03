#include "signalB.h"
#include "IniReader.h"
#include <iostream>

signalB::signalB() {
    // Default values
    double vol_contract_ratio = 0.3;
    double pre_condition_vwap_ratio = 0.995;
    double track_zone_vwap_ratio = 1.01;
    long long buffer_zone_duration_us = 120000000; // 2 min
    double buffer_zone_exit_price_ratio = 1.005;
    double buffer_zone_rolling_short_div = 2.5;
    double buffer_zone_rolling_long_div = 10.0;
    double buffer_zone_day_high_ratio = 0.985;
    long long trade_zone_duration_us = 1200000000; // 20 min
    double trade_zone_eval_price_ratio = 1.005;
    int trade_zone_eval_tick_add = 2;
    int trade_zone_exit_tick_sub = 2;

    IniReader reader;
    if (reader.ReadIni("./cfg/parameter.cfg")) {
        string section = "SignalB";
        try {
            string val;
            val = reader.Read(section.c_str(), "vol_contract_ratio");
            if (!val.empty()) vol_contract_ratio = stod(val);

            val = reader.Read(section.c_str(), "pre_condition_vwap_ratio");
            if (!val.empty()) pre_condition_vwap_ratio = stod(val);

            val = reader.Read(section.c_str(), "track_zone_vwap_ratio");
            if (!val.empty()) track_zone_vwap_ratio = stod(val);

            val = reader.Read(section.c_str(), "buffer_zone_duration_min");
            if (!val.empty()) buffer_zone_duration_us = stoll(val) * 60 * 1000 * 1000;

            val = reader.Read(section.c_str(), "buffer_zone_exit_price_ratio");
            if (!val.empty()) buffer_zone_exit_price_ratio = stod(val);
            
            val = reader.Read(section.c_str(), "buffer_zone_rolling_short_div");
            if (!val.empty()) buffer_zone_rolling_short_div = stod(val);

            val = reader.Read(section.c_str(), "buffer_zone_rolling_long_div");
            if (!val.empty()) buffer_zone_rolling_long_div = stod(val);

            val = reader.Read(section.c_str(), "buffer_zone_day_high_ratio");
            if (!val.empty()) buffer_zone_day_high_ratio = stod(val);

            val = reader.Read(section.c_str(), "trade_zone_duration_min");
            if (!val.empty()) trade_zone_duration_us = stoll(val) * 60 * 1000 * 1000;

            val = reader.Read(section.c_str(), "trade_zone_eval_price_ratio");
            if (!val.empty()) trade_zone_eval_price_ratio = stod(val);

            val = reader.Read(section.c_str(), "trade_zone_eval_tick_add");
            if (!val.empty()) trade_zone_eval_tick_add = stoi(val);

            val = reader.Read(section.c_str(), "trade_zone_exit_tick_sub");
            if (!val.empty()) trade_zone_exit_tick_sub = stoi(val);

        } catch (const std::exception& e) {
            std::cerr << "Error parsing config for SignalB: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Failed to read ./cfg/parameter.cfg" << std::endl;
    }

    config.set(
        vol_contract_ratio,
        pre_condition_vwap_ratio,
        track_zone_vwap_ratio,
        buffer_zone_duration_us,
        buffer_zone_exit_price_ratio,
        buffer_zone_rolling_short_div,
        buffer_zone_rolling_long_div,
        buffer_zone_day_high_ratio,
        trade_zone_duration_us,
        trade_zone_eval_price_ratio,
        trade_zone_eval_tick_add,
        trade_zone_exit_tick_sub
    );
}

bool signalB::eval(IndexData &idx, format6Type *f6) {
    
    

    if (symbol.empty())
        symbol = f6->symbol;
    // trade_zone_eval(idx, f6); // for debug
    // return false;
    // cout << symbol << " trade_zone_eval: Price=" << f6->match.Price << ", trigger_price=" << trade_zone_trigger_price << ", priceCond " << getPriceCond(symbol, f6->match.Price, config.trade_zone_eval_tick_add) << '\n';
    // terminate();
    if (!pre_condition_met(idx, f6)) 
        return false;

    bool trigger = false;
    if (track_zone_eval(idx, f6)) {
        in_buffer_zone = true;
        buffer_zone_trigger_price = f6->match.Price;
        buffer_zone_start_time = f6->matchTime_us;
    }

    if (in_buffer_zone) {
        if (buffer_zone_exit(idx, f6)) {
            in_buffer_zone = false;
            buffer_zone_trigger_price = -1;
            trade_zone_start_time = -1;
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


bool signalB::pre_condition_met(IndexData &idx, format6Type *f6) {
    // (當天未跌破 VWAP × 0.995) # 當天曾經成交價跌破後Signal B失效
    if (f6->match.Price <= idx.vwap * config.pre_condition_vwap_ratio || forbidden) {
        forbidden = true;
        return false;
    }
    return true;
}

bool signalB::track_zone_eval(IndexData &idx, format6Type *f6) {
    bool cond1 = idx.vwap <= f6->match.Price && f6->match.Price <= idx.vwap * config.track_zone_vwap_ratio;
    bool cond2 = f6->match.Price <= idx.rolling_low;

    return cond1 && cond2;
}

bool signalB::buffer_zone_exit(IndexData &idx, format6Type *f6) {
    // Price >= Day High × (1 - 0.5%) (回檔失敗)
    bool cond1 = f6->matchTime_us - buffer_zone_start_time > config.buffer_zone_duration_us;
    bool cond2 = f6->match.Price >= buffer_zone_trigger_price * config.buffer_zone_exit_price_ratio;
    
    return cond1 || cond2;
}

bool signalB::buffer_zone_eval(IndexData &idx, format6Type *f6) {
    bool cond1 = ((double) idx.rolling_sum_ratio) < config.vol_contract_ratio;
    bool cond2 = f6->match.Price <= idx.day_high * config.buffer_zone_day_high_ratio;

    return cond1 && cond2;
}

bool signalB::trade_zone_exit(IndexData &idx, format6Type *f6) {
    bool cond1 = f6->matchTime_us - trade_zone_start_time > config.trade_zone_duration_us;
    bool cond2 = f6->match.Price <= getPriceCond(symbol, trade_zone_trigger_price, -config.trade_zone_exit_tick_sub);
    return cond1 || cond2;
}

bool signalB::trade_zone_eval(IndexData &idx, format6Type *f6) {
    int64_t eval_price = getPriceCond(symbol, f6->match.Price, config.trade_zone_eval_tick_add);
    bool cond1 = f6->match.Price >= trade_zone_trigger_price * config.trade_zone_eval_price_ratio;
    bool cond2 = f6->match.Price >= eval_price;
    
    cout << " symbol " << symbol << " f6->match.Price=" << f6->match.Price << ", trade_zone_trigger_price=" << trade_zone_trigger_price << ", eval_price=" << eval_price << '\n';
    return cond1 && cond2;
}

bool signalB::leave(IndexData &idx, format6Type *f6) {
    // if (enter_market) {
    //     // 停損條件: Price <= trigger_price - 2 ticks 
    //
    // }
    return false;
}