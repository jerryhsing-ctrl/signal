#include "strongGroup.h"
#include "indexCalc.h"
#include "type.h"
#include "QuoteSv.h"

StrongGroup::StrongGroup (){
    readFile("./files/group.csv");

    
}

void StrongGroup::getData() {
    for (auto &[group, members] : group_member) {
        
        for (const string &symbol : members) {
            long long tmp = 0;
            for (int i = 1; i <= DAY_PER_MONTH; i++) 
                tmp += quoteSv->trading_val[i][symbol];
            tmp /= DAY_PER_MONTH;

            tradingValue_monthAvg[symbol] = tmp;
            
            if (group == "電線電纜")
                cout <<  " symbol " << symbol << " value " << tradingValue_monthAvg[symbol] << '\n';
            if (tmp >= 100'000'000)  
                group_tradingValue_monthAvg_sum[group] += tmp;
        }
    }
}

bool StrongGroup::on_tick(IndexData &idx, format6Type *f6) {
    if (symbol_to_group.count(f6->symbol)) 
        return false;
    // cout << "f6 Qty " << f6->match.Qty << " price " << f6->match.Price << '\n'; 


    tradingValue_cumu[f6->symbol] += f6->match.Price * f6->match.Qty;
    group_tradingValue_cumu[symbol_to_group[f6->symbol]] += f6->match.Price * f6->match.Qty;
    price_last[f6->symbol] = f6->match.Price;
    vol_cumu[f6->symbol] += f6->match.Qty;


    if (isValidGroup(idx, f6)) {
        // (個股量增比(參數13) > 1.5 && 當前漲幅 >= 族群平均漲幅-0.5%) || 個股當前漲幅 > 4%
        long long total_vol = 0;
        for (int i = 1; i <= DAY_PER_MONTH; i++) {
            long long vol = quoteSv->vol_cum[i].query(f6->symbol, f6->matchTime_us);
            total_vol += vol;
        }
        long long avg = total_vol / DAY_PER_MONTH;
        bool cond1 = ((double) vol_cumu[f6->symbol] / avg) >= 1.2;

        double gPerChg = groupPercentageChg(symbol_to_group[f6->symbol]);
        double perChg = percentagChg(f6->symbol, f6->match.Price);
        bool cond2 = perChg >= gPerChg - 0.005;

        bool cond3 = perChg > 0.04;

        long long total_month_tradingVal = 0;
        for (int i = 1; i <= DAY_PER_MONTH; i++) { 
            total_month_tradingVal += quoteSv->trading_val[i][f6->symbol];
        }
        bool cond4 = total_month_tradingVal / DAY_PER_MONTH >= 300'000'000;
        return (cond1 || cond2 || cond3) && cond4;
    }

    return false;
}

double StrongGroup::percentagChg(string symbol, long long price) {
    double prev_close = quoteSv->f1mgr.format1Map[symbol].previous_close * 10'000;
    double percantageChg = ((double) price - prev_close) / prev_close;
    return percantageChg;
}

double StrongGroup::groupPercentageChg(std::string group) {
    double avg_percentageChg = 0;
    for (auto &symbol : group_member[group]) {
        double ratio = (double) tradingValue_cumu[symbol] / (double) group_tradingValue_cumu[group];
        double percantageChg = 0;
        if (price_last.count(symbol))
            percantageChg = percentagChg(symbol, price_last[symbol]);

        avg_percentageChg += percantageChg * ratio;
    }
    return avg_percentageChg;
}

bool StrongGroup::isValidGroup(IndexData &idx, format6Type *f6){
    // 1. 月平均成交金額(參數8)成交金額太低 >= 0.1 billion
    bool cond1 = tradingValue_monthAvg[f6->symbol] >= 100'000'000;
    
    // 2. 族群月均成交金額(參數9) > 3 billion
    string group = symbol_to_group[f6->symbol];
    bool cond2 =  group_tradingValue_monthAvg_sum[group] >= 3'000'000'000;

    // 3. 族群當前平均漲幅
    double avg_percentageChg = groupPercentageChg(group);
    bool cond3 = avg_percentageChg > 0.015;

    
    long long total_tradingValue = 0;
    for (auto &symbol : group_member[group]) {
        for (int i = 1; i <= DAY_PER_MONTH; i++) {
            total_tradingValue += quoteSv->vol_cum[i].query(symbol, f6->matchTime_us);
        }
    }
    double val_ratio = (double) group_tradingValue_cumu[group] / (total_tradingValue / 20);
    bool cond4 = val_ratio > 0.015;
    

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
        if (symbol_to_group.count(symbol)) {
            continue; // 跳過，不加入第二個群組
        }

        symbol_to_group[symbol] = group;
        group_member[group].insert(symbol);
    }

    file.close();
}
