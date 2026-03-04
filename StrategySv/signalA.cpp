#include "signalA.h"
#include "IniReader.h"
#include <iostream>
#include "strongSingle.h"
#include "strongGroup.h"
#include "QuoteSv.h"

signalA::signalA() {
    IniReader reader;
    if (reader.ReadIni("./cfg/parameter.cfg")) {
        string section = "SignalA";
        try {
            string val;

            val = reader.Read(section.c_str(), "vwap_near_ratio");
            if (!val.empty()) config.vwap_near_ratio = stod(val);

            val = reader.Read(section.c_str(), "bounce_ratio");
            if (!val.empty()) config.bounce_ratio = stod(val);

            val = reader.Read(section.c_str(), "entry_start_time");
            if (!val.empty()) config.entry_start_time = stoll(val);

            val = reader.Read(section.c_str(), "entry_end_time");
            if (!val.empty()) config.entry_end_time = stoll(val);

            val = reader.Read(section.c_str(), "pre_condition_start_time");
            if (!val.empty()) config.pre_condition_start_time = stoll(val);

            val = reader.Read(section.c_str(), "pre_condition_vwap_ratio");
            if (!val.empty()) config.pre_condition_vwap_ratio = stod(val);

            val = reader.Read(section.c_str(), "trade_zone_max_increase_ratio");
            if (!val.empty()) config.trade_zone_max_increase_ratio = stod(val);

        } catch (const std::exception& e) {
            std::cerr << "Error parsing config for SignalA: " << e.what() << std::endl;
            exit(1);
        }
    } else {
        std::cerr << "Failed to read ./cfg/parameter.cfg" << std::endl;
        exit(1);
    }
}

bool signalA::eval(IndexData &idx, format6Type *f6, MatchType matchType, MatchType &triggerMatchType, StrongSingle &strongSingle, StrongGroup &strongGroup, QuoteSv *quoteSv) {
    if (symbol.empty())
        symbol = f6->symbol;
    this->quoteSv = quoteSv;

    triggerMatchType = MatchType::None;

    if (triggered) return false;
    if (matchType == MatchType::None) return false;
    if (f6->matchTimeStr < config.entry_start_time || f6->matchTimeStr >= config.entry_end_time)
        return false;
    if (!pre_condition_met(idx, f6)) return false;

    // max increase ratio filter
    double prev_close = quoteSv->f1mgr.format1Map[f6->symbol].previous_close * 10000;
    if (prev_close > 0 && (f6->match.Price - prev_close) / prev_close > config.trade_zone_max_increase_ratio)
        return false;

    double price = f6->match.Price;
    double vwap = idx.vwap;
    if (vwap <= 0) return false;

    double pv_ratio = price / vwap;

    // phase 1: detect price approaching VWAP from above
    if (!near_vwap) {
        if (pv_ratio <= config.vwap_near_ratio) {
            near_vwap = true;
            low_since_near = f6->match.Price;
        }
        return false;
    }

    // track the low after approaching VWAP
    if (f6->match.Price < low_since_near)
        low_since_near = f6->match.Price;

    // phase 2: bounce from the low
    double bounce = (price - low_since_near) / (double)low_since_near;
    if (bounce >= config.bounce_ratio) {
        triggered = true;
        triggerMatchType = matchType;
        return true;
    }
    return false;
}

bool signalA::pre_condition_met(IndexData &idx, format6Type *f6) {
    if (f6->matchTimeStr < config.pre_condition_start_time)
        return true;

    if (f6->match.Price <= idx.vwap * config.pre_condition_vwap_ratio || forbidden) {
        forbidden = true;
        return false;
    }
    return true;
}

bool signalA::leave(IndexData &idx, format6Type *f6) {
    return false;
}
