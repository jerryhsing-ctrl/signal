#include "signalB.h"
#include "IniReader.h"
#include <iostream>
#include "strongSingle.h"
#include "strongGroup.h"

static long long min_to_us_B(double min) {
    return min * 60 * 1000000 + 0.01;
}

signalB::signalB() {
    IniReader reader;
    if (reader.ReadIni("./cfg/parameter.cfg")) {
        string section = "SignalB";
        try {
            string val;

            val = reader.Read(section.c_str(), "vol_contract_ratio");
            if (val.empty()) throw std::runtime_error("vol_contract_ratio missing");
            double vol_contract_ratio = stod(val);

            // Read Index config for rolling
            val = reader.Read(section.c_str(), "ROLLING_LOW_DURATION");
            if (val.empty()) throw std::runtime_error("ROLLING_LOW_DURATION missing");
            double rolling_low_duration = min_to_us_B(std::stod(val));
            rolling_low.setDuration(rolling_low_duration);

            val = reader.Read(section.c_str(), "ROLLING_SUM_SHORT_DURATION");
            if (val.empty()) throw std::runtime_error("ROLLING_SUM_SHORT_DURATION missing");
            double rolling_sum_short_duration = min_to_us_B(std::stod(val));
            rolling_sum_short.setDuration(rolling_sum_short_duration);

            val = reader.Read(section.c_str(), "ROLLING_SUM_LONG_DURATION");
            if (val.empty()) throw std::runtime_error("ROLLING_SUM_LONG_DURATION missing");
            double rolling_sum_long_duration = min_to_us_B(std::stod(val));
            rolling_sum_long.setDuration(rolling_sum_long_duration);

            val = reader.Read(section.c_str(), "pre_condition_vwap_ratio");
            if (val.empty()) throw std::runtime_error("pre_condition_vwap_ratio missing");
            double pre_condition_vwap_ratio = stod(val);

            val = reader.Read(section.c_str(), "track_zone_vwap_ratio");
            if (val.empty()) throw std::runtime_error("track_zone_vwap_ratio missing");
            double track_zone_vwap_ratio = stod(val);

            val = reader.Read(section.c_str(), "track_zone_day_high_ratio");
            if (val.empty()) throw std::runtime_error("track_zone_day_high_ratio missing");
            double track_zone_day_high_ratio = stod(val);

            val = reader.Read(section.c_str(), "buffer_zone_duration_min");
            if (val.empty()) throw std::runtime_error("buffer_zone_duration_min missing");
            long long buffer_zone_duration_us = stoll(val) * 60 * 1000 * 1000;

            val = reader.Read(section.c_str(), "buffer_zone_exit_price_ratio");
            if (val.empty()) throw std::runtime_error("buffer_zone_exit_price_ratio missing");
            double buffer_zone_exit_price_ratio = stod(val);

            val = reader.Read(section.c_str(), "trade_zone_duration_min");
            if (val.empty()) throw std::runtime_error("trade_zone_duration_min missing");
            long long trade_zone_duration_us = stoll(val) * 60 * 1000 * 1000;

            val = reader.Read(section.c_str(), "trade_zone_eval_price_ratio");
            if (val.empty()) throw std::runtime_error("trade_zone_eval_price_ratio missing");
            double trade_zone_eval_price_ratio = stod(val);

            val = reader.Read(section.c_str(), "trade_zone_eval_tick_add");
            if (val.empty()) throw std::runtime_error("trade_zone_eval_tick_add missing");
            int trade_zone_eval_tick_add = stoi(val);

            val = reader.Read(section.c_str(), "trade_zone_eval_day_high_increase_ratio");
            if (val.empty()) throw std::runtime_error("trade_zone_eval_day_high_increase_ratio missing");
            double trade_zone_eval_day_high_increase_ratio = stod(val);

            val = reader.Read(section.c_str(), "trade_zone_exit_tick_sub");
            if (val.empty()) throw std::runtime_error("trade_zone_exit_tick_sub missing");
            int trade_zone_exit_tick_sub = stoi(val);

            config.set(
                vol_contract_ratio,
                rolling_low_duration,
                rolling_sum_short_duration,
                rolling_sum_long_duration,
                pre_condition_vwap_ratio,
                track_zone_vwap_ratio,
                track_zone_day_high_ratio,
                buffer_zone_duration_us,
                buffer_zone_exit_price_ratio,
                trade_zone_duration_us,
                trade_zone_eval_price_ratio,
                trade_zone_eval_tick_add,
                trade_zone_eval_day_high_increase_ratio,
                trade_zone_exit_tick_sub
            );

        } catch (const std::exception& e) {
            std::cerr << "Error parsing config for SignalB: " << e.what() << std::endl;
            exit(1);
        }
    } else {
        std::cerr << "Failed to read ./cfg/parameter.cfg" << std::endl;
        exit(1);
    }
}

bool signalB::eval(IndexData &idx, format6Type *f6, MatchType matchType, MatchType &triggerMatchType, StrongSingle &strongSingle, StrongGroup &strongGroup, QuoteSv *quoteSv, bool isStoppedLoss) {
    if (symbol.empty())
        symbol = f6->symbol;
    this->quoteSv = quoteSv;
    
    long long inner_vol = (f6->tradeAt == 1) ? f6->match.Qty : 0;
    
    rolling_sum_short.update(f6->matchTime_us, inner_vol);
    long long rolling_sum_short_ll = rolling_sum_short.getSum();

    rolling_sum_long.update(f6->matchTime_us, inner_vol);
    long long rolling_sum_long_ll = rolling_sum_long.getSum();

    if (rolling_sum_long_ll != 0) {
        rolling_sum_ratio = ((double) rolling_sum_short_ll / ((double) rolling_sum_long_ll)) * (config.rolling_sum_long_duration / config.rolling_sum_short_duration);
    }
    else 
        rolling_sum_ratio = 0;
    
    rolling_low.update(f6->matchTime_us, f6->match.Price);
    rolling_low_val = rolling_low.getLow();
    idx.rolling_low = rolling_low_val;

    
    if (isStoppedLoss) 
        forbidden = true;
    if (!pre_condition_met(idx, f6))  {
        return false;
    }

    bool trigger = false;
    triggerMatchType = MatchType::None;
    
    

    if (matchType != MatchType::None) {
        if (track_zone_eval(idx, f6)) {
            in_buffer_zone = true;
            buffer_zone_trigger_price = f6->match.Price;
            buffer_zone_start_time = f6->matchTime_us;
            buffer_zone_matchType = matchType;
        }
    }

    if (in_buffer_zone) {
        if (buffer_zone_exit(idx, f6)) {
            in_buffer_zone = false;
            buffer_zone_trigger_price = -1;
            buffer_zone_start_time = -1;
            buffer_zone_matchType = MatchType::None;
        } 
        else if (buffer_zone_eval(idx, f6)) {
            in_trade_zone = true;
            trade_zone_trigger_price = buffer_zone_trigger_price;
            trade_zone_start_time = f6->matchTime_us;
            trade_zone_matchType = buffer_zone_matchType;
        
        }
    }

    if (in_trade_zone) {
        if (trade_zone_exit(idx, f6)) {
            in_trade_zone = false;
            trade_zone_trigger_price = -1;
            trade_zone_start_time = -1;
            trade_zone_matchType = MatchType::None;
        } 
        else if (trade_zone_eval(idx, f6)) {
            trigger = true;
            enter_market = true;
            triggerMatchType = trade_zone_matchType;
        }
    }

    return trigger;
}


bool signalB::pre_condition_met(IndexData &idx, format6Type *f6) {
    if (f6->matchTimeStr < 9'20'00'000'000LL)
        return true;

    if (f6->match.Price <= idx.vwap * config.pre_condition_vwap_ratio || forbidden) {
        forbidden = true;
        return false;
    }
    return true;
}

bool signalB::track_zone_eval(IndexData &idx, format6Type *f6) {
    // bool cond1 = idx.vwap <= f6->match.Price && f6->match.Price <= idx.vwap * config.track_zone_vwap_ratio;
    bool cond1 = f6->match.Price <= idx.day_high * config.track_zone_day_high_ratio && f6->match.Price >= idx.vwap * config.track_zone_vwap_ratio;
    bool cond2 = f6->match.Price <= rolling_low_val;

    return cond1 && cond2;
}

bool signalB::buffer_zone_exit(IndexData &idx, format6Type *f6) {
    // Price >= Day High × (1 - 0.5%) (回檔失敗)
    bool cond1 = f6->matchTime_us - buffer_zone_start_time > config.buffer_zone_duration_us;
    bool cond2 = f6->match.Price >= buffer_zone_trigger_price * config.buffer_zone_exit_price_ratio;
    
    return cond1 || cond2;
}

bool signalB::buffer_zone_eval(IndexData &idx, format6Type *f6) {
    if (f6->matchTimeStr < 9'20'00'000'000LL || f6->matchTimeStr >= 11'00'00'000'000LL)
        return false;
    
    bool cond1 = rolling_sum_ratio < config.vol_contract_ratio;
    // bool cond2 = f6->match.Price <= idx.day_high * config.buffer_zone_day_high_ratio && f6->match.Price >= idx.vwap * config.buffer_zone_vwap_ratio;

    return cond1;
}

bool signalB::trade_zone_exit(IndexData &idx, format6Type *f6) {
    
    bool cond1 = f6->matchTime_us - trade_zone_start_time > config.trade_zone_duration_us;
    bool cond2 = f6->match.Price <= getPriceCond(symbol, trade_zone_trigger_price, -config.trade_zone_exit_tick_sub);

    return cond1 || cond2;
}

bool signalB::trade_zone_eval(IndexData &idx, format6Type *f6) {
    if (f6->matchTimeStr < 92'000'000'000)
        return false;
    bool cond1 = f6->match.Price >= trade_zone_trigger_price * config.trade_zone_eval_price_ratio;

    int64_t eval_price = getPriceCond(symbol, trade_zone_trigger_price, config.trade_zone_eval_tick_add);
    bool cond2 = f6->match.Price >= eval_price;

    double prev_close = quoteSv->f1mgr.format1Map[f6->symbol].previous_close * 10'000;
    // cout << " idx.day_high: " << idx.day_high << " prev_close: " << prev_close << " 漲幅 "  << (idx.day_high - prev_close) / prev_close << '\n';
    bool cond3 = (idx.day_high - prev_close) / prev_close > config.trade_zone_eval_day_high_increase_ratio;    
    
    return cond1 && cond2 && cond3;
}

