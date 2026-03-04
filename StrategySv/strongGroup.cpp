#include "strongGroup.h"
#include "indexCalc.h"
#include "type.h"
#include "QuoteSv.h"
#include "IniReader.h"
#include <iostream>

StrongGroup::StrongGroup (){
    readFile("./files/group.csv");

    IniReader reader;
    if (reader.ReadIni("./cfg/parameter.cfg")) {
        string section = "StrongGroup";
        try {
            string val;

            val = reader.Read(section.c_str(), "member_min_month_trading_val");
            if (val.empty()) throw std::runtime_error("member_min_month_trading_val missing");
            config.member_min_month_trading_val = stoll(val);

            val = reader.Read(section.c_str(), "group_min_month_trading_val");
            if (val.empty()) throw std::runtime_error("group_min_month_trading_val missing");
            config.group_min_month_trading_val = stoll(val);

            val = reader.Read(section.c_str(), "group_min_avg_pct_chg");
            if (val.empty()) throw std::runtime_error("group_min_avg_pct_chg missing");
            config.group_min_avg_pct_chg = stod(val);

            val = reader.Read(section.c_str(), "group_min_val_ratio");
            if (val.empty()) throw std::runtime_error("group_min_val_ratio missing");
            config.group_min_val_ratio = stod(val);

            val = reader.Read(section.c_str(), "member_strong_vol_ratio");
            if (val.empty()) throw std::runtime_error("member_strong_vol_ratio missing");
            config.member_strong_vol_ratio = stod(val);

            val = reader.Read(section.c_str(), "member_strong_trading_val");
            if (val.empty()) throw std::runtime_error("member_strong_trading_val missing");
            config.member_strong_trading_val = stoll(val);

            val = reader.Read(section.c_str(), "top_group_rank_threshold");
            if (val.empty()) throw std::runtime_error("top_group_rank_threshold missing");
            config.top_group_rank_threshold = stoi(val);

            val = reader.Read(section.c_str(), "top_group_max_select");
            if (val.empty()) throw std::runtime_error("top_group_max_select missing");
            config.top_group_max_select = stoi(val);

            val = reader.Read(section.c_str(), "top_group_min_select");
            if (val.empty()) throw std::runtime_error("top_group_min_select missing");
            config.top_group_min_select = stoi(val);
            
            val = reader.Read(section.c_str(), "normal_group_max_select");
            if (val.empty()) throw std::runtime_error("normal_group_max_select missing");
            config.normal_group_max_select = stoi(val);

            val = reader.Read(section.c_str(), "normal_group_min_select");
            if (val.empty()) throw std::runtime_error("normal_group_min_select missing");
            config.normal_group_min_select = stoi(val);

            val = reader.Read(section.c_str(), "member_vwap_pct_chg_threshold");
            if (val.empty()) throw std::runtime_error("member_vwap_pct_chg_threshold missing");
            config.member_vwap_pct_chg_threshold = stod(val);

            val = reader.Read(section.c_str(), "group_valid_top_n");
            if (val.empty()) throw std::runtime_error("group_valid_top_n missing");
            config.group_valid_top_n = stoi(val);

            val = reader.Read(section.c_str(), "is_weighted_avg");
            if (val.empty()) throw std::runtime_error("is_weighted_avg missing");
            config.is_weighted_avg = (val == "true");

            val = reader.Read(section.c_str(), "group_vol_ratio_exempt_threshold");
            if (val.empty()) throw std::runtime_error("group_vol_ratio_exempt_threshold missing");
            config.group_vol_ratio_exempt_threshold = stoll(val);

            val = reader.Read("Order", "filter_prev_day_limit_up");
            if (val.empty()) throw std::runtime_error("filter_prev_day_limit_up missing");
            config.filter_prev_day_limit_up = (val == "true");

            val = reader.Read(section.c_str(), "exclude_prev_limit_up_from_rank");
            config.exclude_prev_limit_up_from_rank = (val == "true");

            val = reader.Read(section.c_str(), "member_cond1_enabled");
            config.member_cond1_enabled = val.empty() || (val == "true");

            val = reader.Read(section.c_str(), "member_cond2_enabled");
            config.member_cond2_enabled = val.empty() || (val == "true");

            val = reader.Read(section.c_str(), "member_cond4_enabled");
            config.member_cond4_enabled = val.empty() || (val == "true");

        } catch (const std::exception& e) {
            std::cerr << "Error parsing config for StrongGroup: " << e.what() << std::endl;
            exit(1);
        }
    } else {
        std::cerr << "Failed to read ./cfg/parameter.cfg" << std::endl;
        exit(1);
    }
}

void StrongGroup::getGroup() {
    for (auto &[group, members] : group_member) {
        

        for (const string &symbol : members) {
            if (symbol_is_valid.count(symbol) == 0) {
                long long tmp = 0;
                for (int i = 1; i <= DAY_PER_MONTH; i++) {
                    tmp += quoteSv->trading_val[i][symbol];
                }
                tmp /= DAY_PER_MONTH;

                tradingValue_monthAvg[symbol] = tmp;
                symbol_is_valid[symbol] = (tmp >= config.member_min_month_trading_val);
            }

            if (symbol_is_valid[symbol]) {
                group_tradingValue_monthAvg_sum[group] += tradingValue_monthAvg[symbol];
                group_member_count[group]++;
            }
        }

        
    }

}

bool StrongGroup::on_tick(IndexData &idx, format6Type *f6) {
    if (symbol_to_groups.count(f6->symbol) == 0 || !symbol_is_valid[f6->symbol]) {
        return false;
    }

    // 原始 VWAP 排名：過濾 member_min_month_trading_val + 漲停/極端漲幅
    double rawVwapPerChg = percentagChg(f6->symbol, idx.vwap);
    if (!f6->isLimitUpLocked && rawVwapPerChg < 0.085) {
        for (const auto& group : symbol_to_groups[f6->symbol]) {
            group_member_raw_vwapRank[group].on_tick(f6->symbol, rawVwapPerChg);
        }
    } else if (f6->isLimitUpLocked || rawVwapPerChg >= 0.085) {
        for (const auto& group : symbol_to_groups[f6->symbol]) {
            group_member_raw_vwapRank[group].erase(f6->symbol);
        }
    }

    bool isPrevDayLimitUp = config.filter_prev_day_limit_up && quoteSv->prevDayLimitUpMap[f6->symbol];

    tradingValue_cumu[f6->symbol] += f6->match.Price * f6->match.Qty;
    for (const auto& group : symbol_to_groups[f6->symbol]) {
        group_tradingValue_cumu[group] += f6->match.Price * f6->match.Qty;
    }
    price_last[f6->symbol] = f6->match.Price;
    vol_cumu[f6->symbol] += f6->match.Qty;

    double vwapPerChg = percentagChg(f6->symbol, idx.vwap);
    double pricePerChg = percentagChg(f6->symbol, f6->match.Price);


    bool ans = false;
    for (const auto& group : symbol_to_groups[f6->symbol]) {

        if (!isValidGroup(idx, f6, group)) {
            continue;
        }
        {
            double gPerChg = groupPercentageChg(group, config.is_weighted_avg);
            groupRank.on_tick(group, gPerChg);

            if (!groupRank.isTopN(group, config.group_valid_top_n)) {
                continue;
            }

            long long total_vol = 0;
            for (int i = 1; i <= DAY_PER_MONTH; i++) {
                long long vol = quoteSv->vol_cum[i].query(f6->symbol, f6->matchTime_us);
                total_vol += vol;
            }
            long long avg = total_vol / DAY_PER_MONTH;

            // 個股月均成交金額 > 2 billion
            long long total_tradingVal = 0;
            for (int i = 1; i <= DAY_PER_MONTH; i++) {
                total_tradingVal += quoteSv->trading_val[i][f6->symbol];
            }
            // 族群月均成交量加總夠大，豁免量比條件
            bool group_vol_exempt = group_tradingValue_monthAvg_sum[group] > config.group_vol_ratio_exempt_threshold;
            bool cond1 = !config.member_cond1_enabled
                      || group_vol_exempt
                      || ((double) vol_cumu[f6->symbol] / avg) >= config.member_strong_vol_ratio
                      || total_tradingVal / DAY_PER_MONTH > config.member_strong_trading_val;

            bool cond2 = !config.member_cond2_enabled
                      || (pricePerChg > 0.02 && vwapPerChg > 0.01);

            bool cond4 = !config.member_cond4_enabled
                      || percentagChg(f6->symbol, idx.vwap) > config.member_vwap_pct_chg_threshold;

            auto& f1 = quoteSv->f1mgr.format1Map[f6->symbol];
            double rawLimitUp = f1.limit_up_price;
            long long limitUpInt = (long long)(rawLimitUp * 10000 + 0.5);
            bool isAtLimitUp = f6->match.Price >= limitUpInt;
            bool isDisposition = (f1.security == "RR");

            bool excludePrevLimit = isPrevDayLimitUp && config.exclude_prev_limit_up_from_rank;

            if (isAtLimitUp || isDisposition || vwapPerChg >= 0.085 || excludePrevLimit) {
                group_member_vwapRank[group].erase(f6->symbol);
            }
            else if (cond1 && cond2 && cond4) {
                group_member_vwapRank[group].on_tick(f6->symbol, vwapPerChg);

                int maxChoosen = 0;
                if (groupRank.isTopN(group, config.top_group_rank_threshold)) {
                    maxChoosen = config.top_group_max_select;
                }
                else {
                    maxChoosen = config.normal_group_max_select;
                }
                int cnt = 0;

                for (auto &[gain, symbol] : group_member_vwapRank[group].rank_map) {
                    cnt++;
                    if (symbol == f6->symbol && !isPrevDayLimitUp) {
                        ans = true;
                        int gr = groupRank.getRank(group);
                        int rawRank = group_member_raw_vwapRank[group].getRank(f6->symbol);
                        if (!last_match_info.count(f6->symbol)
                            || last_match_info[f6->symbol].member_rank == 0  // isSingleAllowed 暫存，直接覆寫
                            || gr < last_match_info[f6->symbol].group_rank) {
                            std::string m1 = "";
                            if (!group_member_vwapRank[group].rank_map.empty())
                                m1 = group_member_vwapRank[group].rank_map.begin()->second;
                            last_match_info[f6->symbol] = {group, gr, cnt, rawRank, m1};
                        }
                    }
                    if (cnt >= maxChoosen) {
                        break;
                    }
                }
            }
            else {
                group_member_vwapRank[group].erase(f6->symbol);
            }
        }
    }

    return ans;
}

double StrongGroup::percentagChg(string symbol, long long price) {
    double prev_close = quoteSv->f1mgr.format1Map[symbol].previous_close * 10'000;
    double percantageChg = ((double) price - prev_close) / prev_close;
    return percantageChg;
}

double StrongGroup::groupPercentageChg(std::string group, bool weighted_avg) {
    double avg_percentageChg = 0;
    for (auto &symbol : group_member[group]) {
        double ratio;
        if (weighted_avg)
            ratio = (double) tradingValue_cumu[symbol] / (double) group_tradingValue_cumu[group];
        else
            ratio = 1.0 / group_member_count[group];
        double percantageChg = 0;
        if (price_last.count(symbol))
            percantageChg = percentagChg(symbol, price_last[symbol]);

        avg_percentageChg += percantageChg * ratio;
    }
    return avg_percentageChg;
}

// A  1000   1000
// B   500   250  750

// A+B = 1500

bool StrongGroup::isValidGroup(IndexData &idx, format6Type *f6, const std::string& group){
    // 1. 月平均成交金額(參數8)成交金額太低 >= 0.1 billion
    bool cond1 = tradingValue_monthAvg[f6->symbol] >= config.member_min_month_trading_val;

    // 2. 族群月均成交金額(參數9) > 3 billion
    bool cond2 =  group_tradingValue_monthAvg_sum[group] >=  config.group_min_month_trading_val;

    // 3. 族群當前平均漲幅
    double avg_percentageChg = groupPercentageChg(group, config.is_weighted_avg);
    bool cond3 = avg_percentageChg > config.group_min_avg_pct_chg;



    double val_ratio = (double) group_tradingValue_cumu[group] / (group_tradingValue_monthAvg_sum[group]);
    bool cond4 = val_ratio > config.group_min_val_ratio;

    return cond1 && cond2 && cond3 && cond4;

}

void StrongGroup::readFile(std::string filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        errorLog("Error: Could not open file " + filename);
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line);
        std::string group;
        std::string symbol; // 現在 symbol 會是乾淨的
        std::string name;

      try {
            // (1) symbol
            if (!std::getline(ss, group, ',')) { continue; }
            trim(group);

            // (2) name
            if (!std::getline(ss, symbol, ',')) { continue; }
            trim(symbol);

            // (3) market
            if (!std::getline(ss, name, ',')) { continue; }
            trim(name);

        } catch (const std::exception& e) {
            errorLog("轉換失敗，例如欄位");
        }
        
        symbol_to_groups[symbol].push_back(group);
        group_member[group].insert(symbol);
    }

    file.close();
}

bool StrongGroup::isSingleAllowed(const std::string& symbol, int maxRank) {
    if (symbol_to_groups.count(symbol) == 0)
        return true;  // 沒有歸類在任何族群

    bool has_valid_group = false;
    for (const auto& group : symbol_to_groups[symbol]) {
        // 族群月均成交量不達標 → 跳過（不算有效族群）
        if (group_tradingValue_monthAvg_sum[group] < config.group_min_month_trading_val)
            continue;

        has_valid_group = true;
        auto it = group_member_raw_vwapRank.find(group);
        if (it != group_member_raw_vwapRank.end()) {
            int rank = it->second.getRank(symbol);
            if (rank >= 1 && rank <= maxRank) {
                // 填入 last_match_info 供 report 使用
                int gr = groupRank.getRank(group);
                if (gr < 0) gr = 0;
                last_match_info[symbol] = {group, gr, 0, rank};
                return true;
            }
        }
    }
    return !has_valid_group;  // 所有族群都不達標 → 豁免
}
