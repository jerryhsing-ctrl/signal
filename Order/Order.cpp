#include "Order.h"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <typeinfo>
#include <cctype>
#include "errorLog.h"
#include "tick.h"
#include "IniReader.h"

using namespace std;
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
    const std::string path = "./log/order_log_" + datePart + "_" + sym + ".csv";
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
           << setw(15) << "Cash" << ","
           << setw(15) << "SymbolCash" << ","
           << setw(15) << "SignalType" << ","
           << setw(15) << "EnterCause" << ","
           << setw(15) << "LeaveCause" << ","
           << setw(15) << "RemainingQty" << endl;
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

    const string path = "./log/order_log" + suffix + ".csv";
    logFile.open(path, ios::out | ios::trunc);
    if (!logFile.is_open()) {
        cerr << "Failed to open " << path << endl;
        return;
    }

    // header
    logFile << left << setw(10) << "Action" << ","
            << setw(10) << "Symbol" << ","
            << setw(20) << "Time" << ","
            << setw(15) << "Cash" << ","
            << setw(15) << "SymbolCash" << ","
            << setw(15) << "SignalType" << ","
            << setw(15) << "EnterCause" << ","
            << setw(15) << "LeaveCause" << ","
            << setw(15) << "RemainingQty" << endl;
    logFile.flush();
}

Order::Order() {
    // openLogFile();
    // iniReader = IniReader("./exec/cfg/parameter.cfg");
    reader.ReadIni("./cfg/parameter.cfg");
    string posSizeStr = reader.Read("Order", "position_cash");
    position = stod(posSizeStr);
    cout << "~~~~~~~~~~~~~~~ [" << "position_cash: " << posSizeStr << "]" << endl;
}

Order::~Order() {
    closeSymbolLogFiles();
    if (logFile.is_open()) {
        logFile.flush();
        logFile.close();
    }
}

void Order::setDate(const std::string& date) {
    // if user passes empty/invalid, treat as "not set" -> order_log.csv
    logDate = sanitizeForFilename(date);
    closeSymbolLogFiles(); // date changed -> reopen per-symbol logs with new name
    openLogFile();
}

void Order::trigger(IndexData &idx, format6Type *f6, int entry_idx, SIGNAL_TYPE signal_type, MatchType matchType, StrongSingle &strongSingle, StrongGroup &strongGroup) {
    long long time_limit = 130'000'000'000;
    if (f6->matchTimeStr >= time_limit) {
        return;
    }
    if (f6->prevLimitUp || f6->volatilityPause) {
        return;
    }
    if (stocks[f6->symbol] != 0)
        return;
    if (matchType == MatchType::StrongSingle && strongSingle.forbidden[f6->symbol]) {
        return;
    }
    double currentPrice = (f6->ask[0].Price > 0) ? f6->ask[0].Price : f6->match.Price;
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

    // 鋪賣單
    double q = stocks[f6->symbol] / 5;
    vector<int64_t> prices;
    prices.push_back(getPriceCond(f6->symbol, idx.day_high, -1));
    prices.push_back(idx.day_high);
    prices.push_back(getPriceCond(f6->symbol, idx.day_high, 1));
    prices.push_back(getPriceCond(f6->symbol, idx.day_high, 2));
    prices.push_back(getPriceCond(f6->symbol, idx.day_high, 3));

    orders[f6->symbol].push_back({prices[0], q});
    orders[f6->symbol].push_back({prices[1], q});
    orders[f6->symbol].push_back({prices[2], q});
    orders[f6->symbol].push_back({prices[3], q});
    orders[f6->symbol].push_back({prices[4], q});
    
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

        logFile << left << setw(10) << "enter" << ","
                << setw(10) << f6->symbol << ","
                << setw(20) << f6->matchTimeStr  << ","
                << setw(15) << cash << ","
                << setw(15) << symbolCash[f6->symbol] << ","
                << setw(15) << sigTypeStr << ","
                << setw(15) << cause << ","
                << setw(15) << "-" << ","
                << setw(15) << stocks[f6->symbol]
                << "\n";
        logFile.flush();

        std::ofstream& symLog = getSymbolLogFile(f6->symbol);
        if (symLog.is_open()) {
            symLog << left << setw(10) << "enter" << ","
                   << setw(10) << f6->symbol << ","
                   << setw(20) << f6->matchTimeStr  << ","
                   << setw(15) << cash << ","
                   << setw(15) << symbolCash[f6->symbol] << ","
                   << setw(15) << sigTypeStr << ","
                   << setw(15) << cause << ","
                   << setw(15) << "-" << ","
                   << setw(15) << stocks[f6->symbol]
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

    if (stopLoss(f6)) {
        logFile << left << setw(10) << "leave" << ","
                << setw(10) << f6->symbol << ","
                << setw(20) << f6->matchTimeStr << ","
                << setw(15) << cash << ","
                << setw(15) << symbolCash[f6->symbol] << ","
                << setw(15) << sigTypeStr << ","
                << setw(15) << "-" << ","
                << setw(15) << "stopLoss" << ","
                << setw(15) << stocks[f6->symbol] << "\n";
        logFile.flush();

        std::ofstream& symLog = getSymbolLogFile(f6->symbol);
        if (symLog.is_open()) {
            symLog << left << setw(10) << "leave" << ","
                   << setw(10) << f6->symbol << ","
                   << setw(20) << f6->matchTimeStr << ","
                   << setw(15) << cash << ","
                   << setw(15) << symbolCash[f6->symbol] << ","
                   << setw(15) << sigTypeStr << ","
                   << setw(15) << "-" << ","
                   << setw(15) << "stopLoss" << ","
                   << setw(15) << stocks[f6->symbol] << "\n";
            symLog.flush();
        }
        return;
    }
    else if (timeExit(f6)) {
        logFile << left << setw(10) << "leave" << ","
                << setw(10) << f6->symbol << ","
                << setw(20) << f6->matchTimeStr << ","
                << setw(15) << cash << ","
                << setw(15) << symbolCash[f6->symbol] << ","
                << setw(15) << sigTypeStr << ","
                << setw(15) << "-" << ","
                << setw(15) << "timeExit" << ","
                << setw(15) << stocks[f6->symbol] << "\n";
        logFile.flush();

        std::ofstream& symLog = getSymbolLogFile(f6->symbol);
        if (symLog.is_open()) {
            symLog << left << setw(10) << "leave" << ","
                   << setw(10) << f6->symbol << ","
                   << setw(20) << f6->matchTimeStr << ","
                   << setw(15) << cash << ","
                   << setw(15) << symbolCash[f6->symbol] << ","
                   << setw(15) << sigTypeStr << ","
                   << setw(15) << "-" << ","
                   << setw(15) << "timeExit" << ","
                   << setw(15) << stocks[f6->symbol] << "\n";
            symLog.flush();
        }
        return;
    }
    else if (takeProfit(f6)) {
        logFile << left << setw(10) << "leave" << ","
                << setw(10) << f6->symbol << ","
                << setw(20) << f6->matchTimeStr << ","
                << setw(15) << cash << ","
                << setw(15) << symbolCash[f6->symbol] << ","
                << setw(15) << sigTypeStr << ","
                << setw(15) << "-" << ","
                << setw(15) << "takeProfit" << ","
                << setw(15) << stocks[f6->symbol] << "\n";
        logFile.flush();

        std::ofstream& symLog = getSymbolLogFile(f6->symbol);
        if (symLog.is_open()) {
            symLog << left << setw(10) << "leave" << ","
                   << setw(10) << f6->symbol << ","
                   << setw(20) << f6->matchTimeStr << ","
                   << setw(15) << cash << ","
                   << setw(15) << symbolCash[f6->symbol] << ","
                   << setw(15) << sigTypeStr << ","
                   << setw(15) << "-" << ","
                   << setw(15) << "takeProfit" << ","
                   << setw(15) << stocks[f6->symbol] << "\n";
            symLog.flush();
        }
        // return; // takeProfit不應該return，因為可能同一個tick接著觸發bailout (雖然bailout是先執行的，但如果是下一個tick...)
        // 修正順序：應該先檢查takeProfit，因為當tick價格衝高觸發takeProfit後，同一個tick如果不夠高可能會跌回bailout區? 不太可能。
        // 但如果takeProfit只是部分成交，應該繼續持有。
        // 原本的邏輯有return，這導致如果takeProfit觸發(部分成交)，當次tick就不會檢查bailout。
        // 下一次tick如果價格大跌，就會檢查bailout。
        if (stocks[f6->symbol] == 0) return; // 如果全部賣完了就return
    }
    
    // Check bailout AFTER take profit opportunity (or parallel to it, but bailout needs profitTaken to be true)
    // If takeProfit just set profitTaken=true above, we can check bailout NOW?
    // User question: "Why bailout before takeProfit?"
    // Original code checked bailout BEFORE takeProfit.
    
    if (bailout(f6)) {
        logFile << left << setw(10) << "leave" << ","
                << setw(10) << f6->symbol << ","
                << setw(20) << f6->matchTimeStr << ","
                << setw(15) << cash << ","
                << setw(15) << symbolCash[f6->symbol] << ","
                << setw(15) << sigTypeStr << ","
                << setw(15) << "-" << ","
                << setw(15) << "bailout" << ","
                << setw(15) << stocks[f6->symbol] << "\n";
        logFile.flush();

        std::ofstream& symLog = getSymbolLogFile(f6->symbol);
        if (symLog.is_open()) {
            symLog << left << setw(10) << "leave" << ","
                   << setw(10) << f6->symbol << ","
                   << setw(20) << f6->matchTimeStr << ","
                   << setw(15) << cash << ","
                   << setw(15) << symbolCash[f6->symbol] << ","
                   << setw(15) << sigTypeStr << ","
                   << setw(15) << "-" << ","
                   << setw(15) << "bailout" << ","
                   << setw(15) << stocks[f6->symbol] << "\n";
            symLog.flush();
        }
        return;
    }
    else if (marketClose(f6)) {
        logFile << left << setw(10) << "leave" << ","
                << setw(10) << f6->symbol << ","
                << setw(20) << f6->matchTimeStr << ","
                << setw(15) << cash << ","
                << setw(15) << symbolCash[f6->symbol] << ","
                << setw(15) << sigTypeStr << ","
                << setw(15) << "-" << ","
                << setw(15) << "marketClose" << ","
                << setw(15) << stocks[f6->symbol] << "\n";
        logFile.flush();

        std::ofstream& symLog = getSymbolLogFile(f6->symbol);
        if (symLog.is_open()) {
            symLog << left << setw(10) << "leave" << ","
                   << setw(10) << f6->symbol << ","
                   << setw(20) << f6->matchTimeStr << ","
                   << setw(15) << cash << ","
                   << setw(15) << symbolCash[f6->symbol] << ","
                   << setw(15) << sigTypeStr << ","
                   << setw(15) << "-" << ","
                   << setw(15) << "marketClose" << ","
                   << setw(15) << stocks[f6->symbol] << "\n";
            symLog.flush();
        }
        return;
    }
}


bool Order::stopLoss(format6Type *f6) {
    // Implementation here
    IndexData &entryIdx = entryPointIdx[f6->symbol];
    if ((entrySignalType[f6->symbol] == SIGNAL_TYPE::SIGNAL_A && f6->match.Price <= entryIdx.vwap * 0.997)
        || (entrySignalType[f6->symbol] == SIGNAL_TYPE::SIGNAL_B && f6->match.Price <= entryIdx.rolling_low * 0.997)) {
        stoppedLossSymbols.insert(f6->symbol);
        cancelAll(f6->symbol);
        closeAll(f6->symbol, f6);
        profitTaken[f6->symbol] = false;
        return true;
    }
    return false;
}

bool Order::timeExit(format6Type *f6) {
    // Implementation here
    if (f6->matchTimeStr >= 132'500'000'000) {
        cancelAll(f6->symbol);
        closeAll(f6->symbol, f6);
        profitTaken[f6->symbol] = false;
        return true;
    }   
    return false;
}

bool Order::bailout(format6Type *f6) {
    // Implementation here
    if (!profitTaken[f6->symbol])
        return false;
    

    IndexData &entryIdx = entryPointIdx[f6->symbol];
    if (f6->match.Price <= entryIdx.day_high * 0.992) {
        cancelAll(f6->symbol);
        closeAll(f6->symbol, f6);
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
    if (stocks[f6->symbol] < 0.0000000001)
        stocks[f6->symbol] = 0; // 避免因為浮點數精度問題導致的負數
    if (everTaken)
        profitTaken[f6->symbol] = true;
    return everTaken;
}

bool Order::marketClose(format6Type *f6) {
    if (f6->matchTimeStr >= 132'500'000'000) {
        
        double income = (double) stocks[f6->symbol] * f6->match.Price / ZERO_NUM; // Convert back to actual price
        cash += income;
        symbolCash[f6->symbol] += income;
        stocks[f6->symbol] = 0;

        orders[f6->symbol].clear();
        profitTaken[f6->symbol] = false;
        return true;
    }
    return false;
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

}
