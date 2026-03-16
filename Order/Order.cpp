#include "Order.h"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <typeinfo>
#include <cctype>
#include "errorLog.h"
#include "tick.h"
#include "IniReader.h"
#include <filesystem>
#include "QuoteSv.h"

using namespace std;
namespace fs = std::filesystem;
#define ZERO_NUM 10000.0

static std::string sanitizeForFilename(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '_' || c == '-') {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

void Order::closeSymbolLogFiles() {
    for (auto& kv : symbolLogFiles) {
        if (kv.second && kv.second->is_open()) {
            kv.second->flush();
            kv.second->close();
        }
    }
    symbolLogFiles.clear();
}

std::ofstream& Order::getSymbolLogFile(const std::string& symbol) {
    const std::string sym = sanitizeForFilename(symbol);
    auto it = symbolLogFiles.find(sym);
    if (it != symbolLogFiles.end() && it->second) {
        return *it->second;
    }

    auto ofs = std::make_unique<std::ofstream>();
    const std::string datePart = logDate.empty() ? "unknown" : logDate;
    
    // Use logDir if set, otherwise fallback to ./log/
    string dir = logDir.empty() ? "./log/" : logDir;
    const std::string path = dir + "order_log_" + datePart + "_" + sym + ".csv";
    
    ofs->open(path, ios::out | ios::trunc);
    if (!ofs->is_open()) {
        cerr << "Failed to open " << path << endl;
        // return a reference to main logFile as a fallback sink
        return logFile;
    }

    // header (same as main)
    (*ofs) << left << setw(10) << "Action" << ","
           << setw(10) << "Symbol" << ","
           << setw(20) << "Time" << ","
           << setw(15) << "Price" << ","
           << setw(15) << "Cash" << ","
           << setw(15) << "SymbolCash" << ","
           << setw(15) << "SignalType" << ","
           << setw(15) << "EnterCause" << ","
           << setw(15) << "LeaveCause" << ","
           << setw(15) << "RemainingQty" << ","
           << "GroupInfo"
           << endl;
    ofs->flush();

    auto [insIt, _] = symbolLogFiles.emplace(sym, std::move(ofs));
    return *insIt->second;
}

void Order::openLogFile() {
    if (logFile.is_open()) {
        logFile.flush();
        logFile.close();
    }

    string suffix;
    if (!logDate.empty()) {
        suffix = "_" + logDate;
    }

    string dir = logDir.empty() ? "./log/" : logDir;
    const string path = dir + "order_log" + suffix + ".csv";
    logFile.open(path, ios::out | ios::trunc);
    if (!logFile.is_open()) {
        cerr << "Failed to open " << path << endl;
        return;
    }

    // header
    logFile << left << setw(10) << "Action" << ","
            << setw(10) << "Symbol" << ","
            << setw(20) << "Time" << ","
            << setw(15) << "Price" << ","
            << setw(15) << "Cash" << ","
            << setw(15) << "SymbolCash" << ","
            << setw(15) << "SignalType" << ","
            << setw(15) << "EnterCause" << ","
            << setw(15) << "LeaveCause" << ","
            << setw(15) << "RemainingQty" << ","
            << "GroupInfo"
            << endl;
    logFile.flush();
}

Order::Order() {
    // openLogFile();
    // iniReader = IniReader("./exec/cfg/parameter.cfg");
    reader.ReadIni("./cfg/parameter.cfg");
    string posSizeStr = reader.Read("Order", "position_cash");
    position = stod(posSizeStr);
    cout << "~~~~~~~~~~~~~~~ [" << "position_cash: " << posSizeStr << "]" << endl;
    string dispositionStr = reader.Read("Order", "disposition_stocks_enabled");
    dispostion_enabled = (dispositionStr == "true");
    cout << "~~~~~~~~~~~~~~~ [" << "disposition_stocks_enabled: " << dispositionStr << "]" << endl;
    string filterLimitUpStr = reader.Read("Order", "filter_prev_day_limit_up");
    filter_prev_day_limit_up = (filterLimitUpStr == "true");
    cout << "~~~~~~~~~~~~~~~ [" << "filter_prev_day_limit_up: " << filterLimitUpStr << "]" << endl;

    string val;
    val = reader.Read("Order", "stop_loss_ratio_a");
    if (!val.empty()) stop_loss_ratio_a = stod(val);
    val = reader.Read("Order", "stop_loss_ratio_b");
    if (!val.empty()) stop_loss_ratio_b = stod(val);
    val = reader.Read("Order", "bailout_ratio");
    if (!val.empty()) bailout_ratio = stod(val);
    val = reader.Read("Order", "max_entry_price");
    if (!val.empty()) max_entry_price = stod(val);
    val = reader.Read("Order", "entry_time_limit");
    if (!val.empty()) entry_time_limit = stoll(val);
    val = reader.Read("Order", "exit_time_limit");
    if (!val.empty()) exit_time_limit = stoll(val);
    val = reader.Read("Order", "take_profit_splits");
    if (!val.empty()) take_profit_splits = stoi(val);
    val = reader.Read("Order", "take_profit_tick_offsets");
    if (!val.empty()) {
        take_profit_tick_offsets.clear();
        stringstream ss(val);
        string token;
        while (getline(ss, token, ',')) {
            take_profit_tick_offsets.push_back(stoi(token));
        }
    }
    val = reader.Read("Order", "take_profit_pcts");
    if (!val.empty()) {
        take_profit_pcts.clear();
        stringstream ss(val);
        string token;
        while (getline(ss, token, ',')) {
            take_profit_pcts.push_back(stod(token));
        }
    }
    val = reader.Read("Order", "reserve_limit_up_splits");
    if (!val.empty()) reserve_limit_up_splits = stoi(val);
    val = reader.Read("Order", "tp_base_entry");
    if (!val.empty()) tp_base_entry = (val == "true" || val == "1");
}

Order::~Order() {
    closeSymbolLogFiles();
    if (logFile.is_open()) {
        logFile.flush();
        logFile.close();
    }
}

void Order::setDate(const std::string& date, const std::string& logFolder) {
    // if user passes empty/invalid, treat as "not set" -> order_log.csv
    logDate = sanitizeForFilename(date);

    if (!logFolder.empty()) {
        // Use the folder name passed from shell (MMDD_HHMM) + date subfolder
        logDir = "./log/" + logFolder + "/" + logDate + "/";
    } else {
        // Fallback: create folder with date_HHMM
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm* time_parts = std::localtime(&now_c);
        
        char buffer[10];
        std::strftime(buffer, sizeof(buffer), "%H%M", time_parts);
        logDir = "./log/" + logDate + "_" + buffer + "/";
    }

    try {
        fs::create_directories(logDir);
    } catch(const std::exception& e) {
        cerr << "Failed to create directory: " << logDir << " Error: " << e.what() << endl;
    }

    closeSymbolLogFiles(); // date changed -> reopen per-symbol logs with new name
    openLogFile();

    // open tick dump file
    if (tickDumpFile.is_open()) tickDumpFile.close();
    tickDumpFile.open(logDir + "tick_dump.csv");
    enteredSymbols.clear();
}

void Order::trigger(IndexData &idx, format6Type *f6, int entry_idx, SIGNAL_TYPE signal_type, MatchType matchType, StrongSingle &strongSingle, StrongGroup &strongGroup) {
    if (f6->matchTimeStr >= entry_time_limit) {
        return;
    }
    if (filter_prev_day_limit_up && f6->prevLimitUp) {
        return;
    }
    if (dispostion_enabled) {
        if (f6->volatilityPause)
            return;
    }
    if (stocks[f6->symbol] != 0)
        return;
    if (matchType == MatchType::StrongSingle && strongSingle.forbidden[f6->symbol]) {
        return;
    }
    double currentPrice = (f6->ask[0].Price > 0) ? f6->ask[0].Price : f6->match.Price;
    if (max_entry_price > 0 && currentPrice / ZERO_NUM > max_entry_price)
        return;
    currentPrice /= ZERO_NUM; // Convert back to actual price
    stocks[f6->symbol] = position / currentPrice; // Calculate quantity based on fixed cash position
    entrySignalType[f6->symbol] = signal_type;
    profitTaken[f6->symbol] = false; // Reset profitTaken status for new position
    string sigTypeStr = "";
    if (signal_type == SIGNAL_TYPE::SIGNAL_A) sigTypeStr = "SignalA";
    else if (signal_type == SIGNAL_TYPE::SIGNAL_B) sigTypeStr = "SignalB";
    else if (signal_type == SIGNAL_TYPE::SIGNAL_BOTH) sigTypeStr = "SignalBoth";




    cash -= position;
    symbolCash[f6->symbol] -= position;
    entryPointIdx[f6->symbol] = idx;

    // record open trade for report
    {
        OpenTrade ot;
        ot.symbol = f6->symbol;
        ot.signal_type = sigTypeStr;
        string cause_str = "";
        if (matchType == MatchType::StrongSingle) cause_str = "StrongSingle";
        else if (matchType == MatchType::StrongGroup) cause_str = "StrongGroup";
        else if (matchType == MatchType::Both) cause_str = "Both";
        ot.enter_cause = cause_str;
        ot.entry_time_raw = f6->matchTimeStr;
        ot.baseline = symbolCash[f6->symbol] + position;
        ot.had_take_profit = false;
        auto mi = strongGroup.last_match_info.find(f6->symbol);
        if (mi != strongGroup.last_match_info.end()) {
            ot.group_name = mi->second.group_name;
            ot.group_rank = mi->second.group_rank;
            ot.member_rank = mi->second.member_rank;
            ot.raw_member_rank = mi->second.raw_member_rank;
            ot.m1_symbol = mi->second.m1_symbol;
            ot.vol_ratio = mi->second.vol_ratio;
            ot.month_trading_val = mi->second.month_trading_val;
        }
        ot.entry_price = currentPrice;
        ot.entry_vwap = idx.vwap / 10000.0;
        ot.day_high_at_entry = idx.day_high / 10000.0;
        ot.is_prev_day_lu = f6->prevLimitUp;
        if (quoteSv) {
            auto f1it = quoteSv->f1mgr.format1Map.find(f6->symbol);
            if (f1it != quoteSv->f1mgr.format1Map.end()) {
                ot.prev_close = f1it->second.previous_close;
                ot.is_disposition = (f1it->second.security == "RR");
            }
            ot.had_circuit_breaker = quoteSv->circuitBreakerSymbols.count(f6->symbol) > 0;
        }
        if (!ot.group_name.empty())
            ot.group_limit_up_count = strongGroup.getGroupLimitUpCount(ot.group_name);
        if (p0050_prev > 0 && p0050_latest > 0)
            ot.market_entry_chg_pct = (double)(p0050_latest - p0050_prev) / p0050_prev * 100.0;
        openTrades[f6->symbol] = ot;

        // write ENTRY to tick dump
        if (tickDumpFile.is_open()) {
            enteredSymbols.insert(f6->symbol);
            tickDumpFile << "ENTRY," << f6->symbol
                << "," << f6->matchTimeStr
                << "," << f6->match.Price
                << "," << (long long)idx.vwap
                << "," << idx.day_high
                << "," << (long long)(ot.prev_close * 10000)
                << "," << (long long)stocks[f6->symbol]
                << "," << sigTypeStr
                << "," << (long long)position
                << "," << ot.group_name
                << "," << ot.group_rank
                << "," << ot.member_rank
                << "," << ot.raw_member_rank
                << "," << pending_near_vwap_time
                << "," << std::fixed << std::setprecision(6) << pending_near_vwap_pv_ratio
                << "\n";
        }
    }

    // 鋪賣單
    int actual_splits = take_profit_splits + reserve_limit_up_splits;
    double q = stocks[f6->symbol] / actual_splits;
    vector<int64_t> prices;

    // Get limit-up price
    long long limitUpInt = 0;
    if (quoteSv) {
        auto it = quoteSv->f1mgr.format1Map.find(f6->symbol);
        if (it != quoteSv->f1mgr.format1Map.end()) {
            limitUpInt = (long long)(it->second.limit_up_price * 10000 + 0.5);
        }
    }

    if (!take_profit_pcts.empty()) {
        // Percentage-based TP
        int64_t tpBase = tp_base_entry ? f6->match.Price : idx.day_high;
        for (int i = 0; i < take_profit_splits && i < (int)take_profit_pcts.size(); i++) {
            int64_t p = (int64_t)(tpBase * (1.0 + take_profit_pcts[i]) + 0.5);
            if (limitUpInt > 0 && p > limitUpInt) p = limitUpInt;
            prices.push_back(p);
        }
    } else {
        // Tick-based TP (original logic)
        for (int i = 0; i < take_profit_splits && i < (int)take_profit_tick_offsets.size(); i++) {
            int offset = take_profit_tick_offsets[i];
            if (offset == 0)
                prices.push_back(idx.day_high);
            else
                prices.push_back(getPriceCond(f6->symbol, idx.day_high, offset));
        }
        // cap at limit-up
        if (limitUpInt > 0) {
            for (auto& p : prices) {
                if (p > limitUpInt) p = limitUpInt;
            }
        }
    }

    // Reserve splits: NOT placed as orders — held until end of day
    reserveStocks[f6->symbol] = q * reserve_limit_up_splits;
    limitUpPrices[f6->symbol] = limitUpInt;

    if (f6->symbol == "2360") {
        cout << " ====== trigger price " << f6->match.Price << " time " << f6->matchTimeStr << " day_high " << idx.day_high << '\n';
        for (size_t i = 0; i < prices.size(); i++)
            cout << " ====== order price " << prices[i] << '\n';
    }

    for (size_t i = 0; i < prices.size(); i++) {
        orders[f6->symbol].push_back({prices[i], q});
    }
    
    if (logFile.is_open() && !f6->symbol.empty()) {
            // 檢查symbol長度是否合理

        
        // 獲取當前時間
        auto now = chrono::system_clock::now();
        auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
        
        string cause = "";
        if (matchType == MatchType::StrongSingle) cause = "StrongSingle";
        else if (matchType == MatchType::StrongGroup) cause = "StrongGroup";
        else if (matchType == MatchType::Both) cause = "Both";
        else cause = "None";

        // group info for enter log
        string groupInfo = "-";
        {
            auto mi = strongGroup.last_match_info.find(f6->symbol);
            if (mi != strongGroup.last_match_info.end()) {
                groupInfo = mi->second.group_name
                    + "(G" + to_string(mi->second.group_rank)
                    + "/M" + to_string(mi->second.member_rank)
                    + "/R" + to_string(mi->second.raw_member_rank) + ")"
                    + (mi->second.member_rank > 1 ? " M1=" + mi->second.m1_symbol : "");
            }
        }

        logFile << left << setw(10) << "enter" << ","
                << setw(10) << f6->symbol << ","
                << setw(20) << f6->matchTimeStr  << ","
                << setw(15) << f6->match.Price << ","
                << setw(15) << cash << ","
                << setw(15) << symbolCash[f6->symbol] << ","
                << setw(15) << sigTypeStr << ","
                << setw(15) << cause << ","
                << setw(15) << "-" << ","
                << setw(15) << stocks[f6->symbol] << ","
                << groupInfo
                << "\n";
        logFile.flush();

        std::ofstream& symLog = getSymbolLogFile(f6->symbol);
        if (symLog.is_open()) {
            symLog << left << setw(10) << "enter" << ","
                   << setw(10) << f6->symbol << ","
                   << setw(20) << f6->matchTimeStr  << ","
                   << setw(15) << f6->match.Price << ","
                   << setw(15) << cash << ","
                   << setw(15) << symbolCash[f6->symbol] << ","
                   << setw(15) << sigTypeStr << ","
                   << setw(15) << cause << ","
                   << setw(15) << "-" << ","
                   << setw(15) << stocks[f6->symbol] << ","
                   << groupInfo
                   << "\n";
            symLog.flush();
        }
    }
    else {
        errorLog("Order::trigger, log file not open or symbol empty");
    }
}

void Order::on_tick(format6Type *f6) {
    if (stocks[f6->symbol] == 0)
        return;
    string sigTypeStr = "-";

    auto recordClose = [&](const string& leaveCause) {
        auto it = openTrades.find(f6->symbol);
        if (it == openTrades.end()) return;
        auto& ot = it->second;
        TradeRecord tr;
        tr.symbol = ot.symbol;
        tr.signal_type = ot.signal_type;
        tr.enter_cause = ot.enter_cause;
        tr.entry_time_raw = ot.entry_time_raw;
        tr.exit_time_raw = f6->matchTimeStr;
        tr.pnl = symbolCash[f6->symbol] - ot.baseline;
        tr.return_pct = tr.pnl / position * 100.0;
        tr.final_leave_cause = leaveCause;
        tr.had_take_profit = ot.had_take_profit;
        tr.group_name = ot.group_name;
        tr.group_rank = ot.group_rank;
        tr.member_rank = ot.member_rank;
        tr.raw_member_rank = ot.raw_member_rank;
        tr.m1_symbol = ot.m1_symbol;
        tr.entry_price = ot.entry_price;
        tr.entry_vwap = ot.entry_vwap;
        tr.day_high_at_entry = ot.day_high_at_entry;
        tr.prev_close = ot.prev_close;
        tr.vol_ratio = ot.vol_ratio;
        tr.month_trading_val = ot.month_trading_val;
        tr.is_prev_day_lu = ot.is_prev_day_lu;
        tr.is_disposition = ot.is_disposition;
        tr.had_circuit_breaker = ot.had_circuit_breaker;
        tr.group_limit_up_count = ot.group_limit_up_count;
        tr.market_entry_chg_pct = ot.market_entry_chg_pct;
        completedTrades.push_back(tr);
        openTrades.erase(it);
    };

    auto writeLeave = [&](const string& cause) {
        logFile << left << setw(10) << "leave" << ","
                << setw(10) << f6->symbol << ","
                << setw(20) << f6->matchTimeStr << ","
                << setw(15) << f6->match.Price << ","
                << setw(15) << cash << ","
                << setw(15) << symbolCash[f6->symbol] << ","
                << setw(15) << sigTypeStr << ","
                << setw(15) << "-" << ","
                << setw(15) << cause << ","
                << setw(15) << stocks[f6->symbol] << "\n";
        logFile.flush();

        std::ofstream& symLog = getSymbolLogFile(f6->symbol);
        if (symLog.is_open()) {
            symLog << left << setw(10) << "leave" << ","
                   << setw(10) << f6->symbol << ","
                   << setw(20) << f6->matchTimeStr << ","
                   << setw(15) << f6->match.Price << ","
                   << setw(15) << cash << ","
                   << setw(15) << symbolCash[f6->symbol] << ","
                   << setw(15) << sigTypeStr << ","
                   << setw(15) << "-" << ","
                   << setw(15) << cause << ","
                   << setw(15) << stocks[f6->symbol] << "\n";
            symLog.flush();
        }
    };

    if (stopLoss(f6)) {
        writeLeave("stopLoss");
        recordClose("stopLoss");
        return;
    }
    else if (timeExit(f6)) {
        writeLeave(lastTimeExitCause);
        recordClose(lastTimeExitCause);
        return;
    }
    else if (takeProfit(f6)) {
        writeLeave("takeProfit");
        // takeProfit 部分成交時繼續持有，全部賣完才 return
        if (openTrades.count(f6->symbol))
            openTrades[f6->symbol].had_take_profit = true;
        double remainReserve = reserveStocks.count(f6->symbol) ? reserveStocks[f6->symbol] : 0;
        if (stocks[f6->symbol] <= 0.001 && remainReserve <= 0.001) {
            stocks[f6->symbol] = 0;
            recordClose("takeProfit");
            return;
        }
    }

    if (bailout(f6)) {
        writeLeave("bailout");
        recordClose("bailout");
        return;
    }
}


bool Order::stopLoss(format6Type *f6) {
    // Implementation here
    IndexData &entryIdx = entryPointIdx[f6->symbol];
    if ((entrySignalType[f6->symbol] == SIGNAL_TYPE::SIGNAL_A && f6->match.Price <= entryIdx.vwap * stop_loss_ratio_a)
        || (entrySignalType[f6->symbol] == SIGNAL_TYPE::SIGNAL_B && f6->match.Price <= entryIdx.rolling_low * stop_loss_ratio_b)) {
        stoppedLossSymbols.insert(f6->symbol);
        cancelAll(f6->symbol);
        closeAll(f6->symbol, f6);
        reserveStocks[f6->symbol] = 0;
        profitTaken[f6->symbol] = false;
        return true;
    }
    return false;
}

bool Order::timeExit(format6Type *f6) {
    if (f6->matchTimeStr >= exit_time_limit) {
        double reserve = reserveStocks.count(f6->symbol) ? reserveStocks[f6->symbol] : 0;
        long long limitUp = limitUpPrices.count(f6->symbol) ? limitUpPrices[f6->symbol] : 0;

        if (reserve > 0.001 && limitUp > 0 && f6->match.Price >= limitUp) {
            // 收盤鎖漲停 → reserve 以漲停價計算
            cancelAll(f6->symbol);
            // 先賣掉非 reserve 部位 at market
            double nonReserve = std::max(0.0, stocks[f6->symbol] - reserve);
            if (nonReserve > 0.001) {
                double marketPrice = (f6->bid[0].Price > 0) ? f6->bid[0].Price : f6->match.Price;
                double income = nonReserve * marketPrice / ZERO_NUM;
                cash += income;
                symbolCash[f6->symbol] += income;
            }
            // reserve 以漲停價賣出（模擬收盤鎖漲停）
            double income = reserve * limitUp / ZERO_NUM;
            cash += income;
            symbolCash[f6->symbol] += income;
            stocks[f6->symbol] = 0;
            reserveStocks[f6->symbol] = 0;
            profitTaken[f6->symbol] = false;
            lastTimeExitCause = "lockedLimitUp";
        } else {
            // 沒鎖漲停 → 全部以市價出場
            cancelAll(f6->symbol);
            closeAll(f6->symbol, f6);
            reserveStocks[f6->symbol] = 0;
            profitTaken[f6->symbol] = false;
            lastTimeExitCause = "timeExit";
        }
        return true;
    }
    return false;
}

bool Order::bailout(format6Type *f6) {
    // Implementation here
    if (!profitTaken[f6->symbol])
        return false;
    

    IndexData &entryIdx = entryPointIdx[f6->symbol];
    if (f6->match.Price <= entryIdx.day_high * bailout_ratio) {
        cancelAll(f6->symbol);
        closeAll(f6->symbol, f6);
        reserveStocks[f6->symbol] = 0;
        profitTaken[f6->symbol] = false;

        return true;
    }
    return false;
}

bool Order::takeProfit(format6Type *f6) {
    // Implementation here
    bool everTaken = false;
    for (int i = orders[f6->symbol].size() - 1; i >= 0; i--) {
        auto& order = orders[f6->symbol][i];
        // long long price = order.first;
        // double qty = order.second;
        auto [price, qty] = order;

        // 檢查是否符合成交條件
        if (f6->match.Price >= price) {
            // 成交
            double income = (double) qty * price / ZERO_NUM; // Convert back to actual price
            cash += income;
            symbolCash[f6->symbol] += income;
            stocks[f6->symbol] -= qty;

            // 移除已成交的訂單
            orders[f6->symbol].erase(orders[f6->symbol].begin() + i);
            everTaken = true;
        }
    }
    if (stocks[f6->symbol] < 0.001)
        stocks[f6->symbol] = 0; // 避免因為浮點數精度問題導致的負數
    if (everTaken)
        profitTaken[f6->symbol] = true;
    return everTaken;
}


void Order::cancelAll(string symbol) {
    // Implementation here
    orders[symbol].clear();
}

void Order::closeAll(string symbol, format6Type *f6) {
    // Implementation here
    double marketPrice = (f6->bid[0].Price > 0) ? f6->bid[0].Price : f6->match.Price;
    marketPrice /= ZERO_NUM; // Convert back to actual price
    double qty = stocks[symbol];
    double income = (double) qty * marketPrice; // Convert back to actual price
    cash += income;
    symbolCash[symbol] += income;

    stocks[symbol] = 0;

    // 修正浮點精度殘餘
    if (std::abs(symbolCash[symbol]) < 1.0) {
        cash -= symbolCash[symbol];
        symbolCash[symbol] = 0;
    }
}

void Order::dumpTick(format6Type *f6) {
    if (!tickDumpFile.is_open()) return;
    if (enteredSymbols.count(f6->symbol) == 0) return;
    tickDumpFile << "TICK," << f6->symbol
        << "," << f6->matchTimeStr
        << "," << f6->match.Price
        << "," << f6->bid[0].Price << "\n";
}

// ── Report Generation ───────────────────────────────────────────────────

static string fmtTime(long long raw) {
    long long total = raw / 1'000'000LL;
    int hh = (int)(total / 10000);
    int mm = (int)((total % 10000) / 100);
    int ss = (int)(total % 100);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hh, mm, ss);
    return string(buf);
}

static int durationSec(long long entry, long long exit) {
    auto toSec = [](long long t) -> int {
        long long ts = t / 1'000'000LL;
        return (int)((ts / 10000) * 3600 + ((ts % 10000) / 100) * 60 + (ts % 100));
    };
    return toSec(exit) - toSec(entry);
}

static string fmtDuration(int sec) {
    int h = sec / 3600;
    int m = (sec % 3600) / 60;
    int s = sec % 60;
    char buf[32];
    snprintf(buf, sizeof(buf), "%dh%02dm%02ds", h, m, s);
    return string(buf);
}

void Order::generateReport() {
    if (completedTrades.empty()) {
        cout << "[Report] No completed trades.\n";
        return;
    }

    string dir = logDir.empty() ? "./log/" : logDir;

    // ── 1. Trade List ──
    {
        string path = dir + "report_trades.csv";
        ofstream f(path);
        f << "Symbol,SignalType,EnterCause,EntryTime,ExitTime,LeaveCause,PnL,Return%,HoldingDuration,"
          << "GroupName,GroupRank,MemberRank,RawMemberRank,M1Symbol,"
          << "EntryPrice,EntryVWAP,DayHigh,PrevClose,0050OpenChg%,"
          << "VolRatio,MonthTradingVal,"
          << "IsPrevDayLU,IsDisposition,HadCircuitBreaker,GroupLimitUpCount,0050EntryChg%\n";
        for (auto& t : completedTrades) {
            int dur = durationSec(t.entry_time_raw, t.exit_time_raw);
            f << t.symbol << ","
              << t.signal_type << ","
              << t.enter_cause << ","
              << fmtTime(t.entry_time_raw) << ","
              << fmtTime(t.exit_time_raw) << ","
              << t.final_leave_cause << ","
              << fixed << setprecision(0) << t.pnl << ","
              << fixed << setprecision(2) << t.return_pct << "%,"
              << fmtDuration(dur) << ","
              << t.group_name << ","
              << t.group_rank << ","
              << t.member_rank << ","
              << t.raw_member_rank << ","
              << (t.member_rank > 1 ? t.m1_symbol : "") << ","
              << fixed << setprecision(2) << t.entry_price << ","
              << fixed << setprecision(2) << t.entry_vwap << ","
              << fixed << setprecision(2) << t.day_high_at_entry << ","
              << fixed << setprecision(2) << t.prev_close << ","
              << fixed << setprecision(3) << market_open_chg_pct << ","
              << fixed << setprecision(2) << t.vol_ratio << ","
              << t.month_trading_val << ","
              << (t.is_prev_day_lu ? 1 : 0) << ","
              << (t.is_disposition ? 1 : 0) << ","
              << (t.had_circuit_breaker ? 1 : 0) << ","
              << t.group_limit_up_count << ","
              << fixed << setprecision(3) << t.market_entry_chg_pct << "\n";
        }
        cout << "[Report] " << path << "\n";
    }

    // ── collect stats ──
    int total = (int)completedTrades.size();
    double totalPnl = 0;
    int winCount = 0;
    double grossWin = 0, grossLoss = 0;
    double maxWin = -1e18, maxLoss = 1e18;
    int maxConsecWin = 0, maxConsecLoss = 0, curConsec = 0;
    bool lastWin = false;

    // cumulative PnL for drawdown
    double cumPnl = 0, peak = 0, maxDD = 0;

    // by category
    unordered_map<string, vector<double>> bySignal, byCause, byLeave;
    double totalHoldSec = 0;

    for (int i = 0; i < total; i++) {
        auto& t = completedTrades[i];
        totalPnl += t.pnl;
        cumPnl += t.pnl;
        if (cumPnl > peak) peak = cumPnl;
        double dd = peak - cumPnl;
        if (dd > maxDD) maxDD = dd;

        if (t.pnl > maxWin) maxWin = t.pnl;
        if (t.pnl < maxLoss) maxLoss = t.pnl;

        bool isWin = t.pnl > 0;
        if (isWin) {
            winCount++;
            grossWin += t.pnl;
        } else {
            grossLoss += t.pnl;
        }

        // consecutive
        if (i == 0) {
            curConsec = 1;
            lastWin = isWin;
        } else if (isWin == lastWin) {
            curConsec++;
        } else {
            curConsec = 1;
            lastWin = isWin;
        }
        if (isWin && curConsec > maxConsecWin) maxConsecWin = curConsec;
        if (!isWin && curConsec > maxConsecLoss) maxConsecLoss = curConsec;

        bySignal[t.signal_type].push_back(t.pnl);
        byCause[t.enter_cause].push_back(t.pnl);
        byLeave[t.final_leave_cause].push_back(t.pnl);
        totalHoldSec += durationSec(t.entry_time_raw, t.exit_time_raw);
    }

    int lossCount = total - winCount;
    double avgWin = winCount > 0 ? grossWin / winCount : 0;
    double avgLoss = lossCount > 0 ? grossLoss / lossCount : 0;
    double profitFactor = (grossLoss != 0) ? grossWin / (-grossLoss) : 0;
    double avgReturn = 0;
    for (auto& t : completedTrades) avgReturn += t.return_pct;
    avgReturn /= total;

    // ── 2. Summary ──
    {
        string path = dir + "report_summary.csv";
        ofstream f(path);
        f << "Metric,Value\n";
        f << "Total Trades," << total << "\n";
        f << "Total PnL," << fixed << setprecision(0) << totalPnl << "\n";
        f << "Win Rate," << fixed << setprecision(1) << (winCount * 100.0 / total) << "%\n";
        f << "Win Count," << winCount << "\n";
        f << "Loss Count," << lossCount << "\n";
        f << "Avg Win," << fixed << setprecision(0) << avgWin << "\n";
        f << "Avg Loss," << fixed << setprecision(0) << avgLoss << "\n";
        f << "Profit Factor," << fixed << setprecision(2) << profitFactor << "\n";
        f << "Max Single Win," << fixed << setprecision(0) << maxWin << "\n";
        f << "Max Single Loss," << fixed << setprecision(0) << maxLoss << "\n";
        f << "Max Consecutive Wins," << maxConsecWin << "\n";
        f << "Max Consecutive Losses," << maxConsecLoss << "\n";
        f << "Max Drawdown," << fixed << setprecision(0) << maxDD << "\n";
        f << "Avg Holding Duration," << fmtDuration((int)(totalHoldSec / total)) << "\n";
        f << "Avg Return%," << fixed << setprecision(2) << avgReturn << "%\n";
        cout << "[Report] " << path << "\n";
    }

    // ── 3. By Category ──
    {
        string path = dir + "report_by_category.csv";
        ofstream f(path);
        f << "Category,Value,Count,WinRate,TotalPnL,AvgPnL\n";

        auto writeGroup = [&](const string& cat, unordered_map<string, vector<double>>& m) {
            for (auto& [key, pnls] : m) {
                int cnt = (int)pnls.size();
                double sum = 0;
                int w = 0;
                for (double p : pnls) { sum += p; if (p > 0) w++; }
                f << cat << "," << key << "," << cnt << ","
                  << fixed << setprecision(1) << (w * 100.0 / cnt) << "%,"
                  << fixed << setprecision(0) << sum << ","
                  << fixed << setprecision(0) << (sum / cnt) << "\n";
            }
        };

        writeGroup("SignalType", bySignal);
        writeGroup("EnterCause", byCause);
        writeGroup("LeaveCause", byLeave);
        cout << "[Report] " << path << "\n";
    }

    // ── terminal summary ──
    cout << "\n========== Backtest Report ==========\n";
    cout << "  Total Trades:    " << total << "\n";
    cout << "  Total PnL:       " << fixed << setprecision(0) << totalPnl << "\n";
    cout << "  Win Rate:        " << fixed << setprecision(1) << (winCount * 100.0 / total) << "%\n";
    cout << "  Profit Factor:   " << fixed << setprecision(2) << profitFactor << "\n";
    cout << "  Max Drawdown:    " << fixed << setprecision(0) << maxDD << "\n";
    cout << "  Avg Return:      " << fixed << setprecision(2) << avgReturn << "%\n";
    cout << "=====================================\n";
}
