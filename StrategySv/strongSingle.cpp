#include "strongSingle.h"
#include "indexCalc.h"
#include "type.h"
#include "QuoteSv.h"
#include "IniReader.h"
#include <iostream>

StrongSingle::StrongSingle () : topTracker(200) {
    IniReader reader;
    if (reader.ReadIni("./cfg/parameter.cfg")) {
        string section = "StrongSignal";
        try {
            string val;

            val = reader.Read(section.c_str(), "monitor_pool_size");
            if (val.empty()) throw std::runtime_error("monitor_pool_size missing");
            config.monitor_pool_size = stoi(val);
            // Re-initialize topTracker with config value
            topTracker = TopKVolumeTracker(config.monitor_pool_size);

            val = reader.Read(section.c_str(), "min_month_trading_val");
            if (val.empty()) throw std::runtime_error("min_month_trading_val missing");
            config.min_month_trading_val = stoll(val);

            val = reader.Read(section.c_str(), "price_amplitude_threshold");
            if (val.empty()) throw std::runtime_error("price_amplitude_threshold missing");
            config.price_amplitude_threshold = stod(val);

            val = reader.Read(section.c_str(), "day_high_increase_threshold");
            if (val.empty()) throw std::runtime_error("day_high_increase_threshold missing");
            config.day_high_increase_threshold = stod(val);

            val = reader.Read(section.c_str(), "vol_increase_month_ratio");
            if (val.empty()) throw std::runtime_error("vol_increase_month_ratio missing");
            config.vol_increase_month_ratio = stod(val);

            val = reader.Read(section.c_str(), "vol_increase_yesterday_ratio");
            if (val.empty()) throw std::runtime_error("vol_increase_yesterday_ratio missing");
            config.vol_increase_yesterday_ratio = stod(val);

            val = reader.Read(section.c_str(), "strong_month_trading_val");
            if (val.empty()) throw std::runtime_error("strong_month_trading_val missing");
            config.strong_month_trading_val = stoll(val);

            val = reader.Read(section.c_str(), "vwap_floor_start_time");
            if (val.empty()) throw std::runtime_error("vwap_floor_start_time missing");
            config.vwap_floor_start_time = stoll(val);

            val = reader.Read(section.c_str(), "vwap_floor_ratio");
            if (val.empty()) throw std::runtime_error("vwap_floor_ratio missing");
            config.vwap_floor_ratio = stod(val);

            val = reader.Read(section.c_str(), "extreme_price_increase_limit");
            if (val.empty()) throw std::runtime_error("extreme_price_increase_limit missing");
            config.extreme_price_increase_limit = stod(val);

        } catch (const std::exception& e) {
            std::cerr << "Error parsing config for StrongSignal: " << e.what() << std::endl;
            exit(1);
        }
    } else {
        std::cerr << "Failed to read ./cfg/parameter.cfg" << std::endl;
        exit(1);
    }
}

bool StrongSingle::on_tick(IndexData &idx, format6Type *f6) {
    if (symbol_is_valid.count(f6->symbol) == 0) {
        long long tmp = 0;
        for (int i = 1; i <= DAY_PER_MONTH; i++) {
            tmp += quoteSv->trading_val[i][f6->symbol];
        }
        tmp /= DAY_PER_MONTH;
        symbol_is_valid[f6->symbol] = (tmp >= config.min_month_trading_val);
    }

    if (symbol_is_valid[f6->symbol] == false)
        return false;

    vol_cumu[f6->symbol] += f6->match.Qty;

    bool inPool = false;
    topTracker.on_tick(f6->symbol, f6->match.Qty * f6->match.Price);
    if (topTracker.inPool(f6->symbol))
        inPool = true;

    
    bool cond1 = eval_price_cond(idx, f6);
    bool cond2 = eval_vol_cond(idx, f6);
    bool cond3 = eval_vwap_cond(idx, f6);
    bool cond4 = eval_extremeFilter_cond(idx, f6);

    // if (f6->symbol == "3189" && inPool &&cond1 && cond2 && cond3 && cond4)
    //     cout << " on_tick, symbol: " << f6->symbol << ", matchTimeStr: " << f6->matchTimeStr << ", price: " << f6->match.Price << ", vwap: " << idx.vwap << ", maxPriceAmp: " << maxPriceAmp[f6->symbol] << '\n';

    return inPool &&cond1 && cond2 && cond3 && cond4;
}

bool StrongSingle::eval_price_cond(IndexData &idx, format6Type *f6) {

    double priceAmp = (double) (idx.day_high - idx.day_low) / idx.day_low;
    maxPriceAmp[f6->symbol] = max(priceAmp, maxPriceAmp[f6->symbol]);

    bool cond1 = false;
    double prev_close = quoteSv->f1mgr.format1Map[f6->symbol].previous_close * 10000;
    bool cond2 = (double) (idx.day_high - prev_close) / prev_close > config.day_high_increase_threshold;

    return cond1 || cond2;
}

bool StrongSingle::eval_vol_cond(IndexData &idx, format6Type *f6) {
    long long total_vol = 0;
    for (int i = 1; i <= DAY_PER_MONTH; i++) {
        long long vol = quoteSv->vol_cum[i].query(f6->symbol, f6->matchTime_us);
        total_vol += vol;
    }
    long long avg = total_vol / DAY_PER_MONTH;


    bool cond1 = ((double) vol_cumu[f6->symbol] / avg) >= config.vol_increase_month_ratio;

    long long cum_vol_yesterday = quoteSv->vol_cum[1].query(f6->symbol, f6->matchTime_us);
    bool cond2 = ((double) vol_cumu[f6->symbol] / cum_vol_yesterday) >= config.vol_increase_yesterday_ratio;

    long long total_tradingVal = 0;
    for (int i = 1; i <= DAY_PER_MONTH; i++) {
        total_tradingVal += quoteSv->trading_val[i][f6->symbol];
    }
    bool cond3 = total_tradingVal / DAY_PER_MONTH > config.strong_month_trading_val;
    
    return cond1 || cond2 || cond3;
}

bool StrongSingle::eval_vwap_cond(IndexData &idx, format6Type *f6) {
    if (forbidden[f6->symbol])
        return false;

    if (f6->matchTimeStr >= config.vwap_floor_start_time){
        if (f6->match.Price <= idx.vwap  * config.vwap_floor_ratio) {
            forbidden[f6->symbol] = true;
        }
    }
    
    return !forbidden[f6->symbol];
}

bool StrongSingle::eval_extremeFilter_cond(IndexData &idx, format6Type *f6) {
    double prev_close = quoteSv->f1mgr.format1Map[f6->symbol].previous_close * 10'000;
    double percentageChg = ((double)f6->match.Price - prev_close) / prev_close;

    return percentageChg < config.extreme_price_increase_limit;
}