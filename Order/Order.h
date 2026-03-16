#pragma once
#include "type.h"
#include "Format6.h"
#include "indexCalc.h"
#include "strongSingle.h"
#include "strongGroup.h"

#include <fstream>
#include <chrono>
#include <vector>
#include <set>
#include <string>
#include <memory>
#include "IniReader.h"

class QuoteSv;

enum class SIGNAL_TYPE {
	SIGNAL_A,
	SIGNAL_B,
    SIGNAL_BOTH
};

struct TradeRecord {
    std::string symbol;
    std::string signal_type;     // "SignalA", "SignalB", "SignalBoth"
    std::string enter_cause;     // "StrongGroup", "StrongSingle", "Both"
    std::string final_leave_cause;
    long long entry_time_raw = 0;
    long long exit_time_raw = 0;
    double pnl = 0;
    double return_pct = 0;
    bool had_take_profit = false;
    std::string group_name;
    int group_rank = 0;     // group's rank among all groups (1-based)
    int member_rank = 0;    // stock's rank within the group (1-based)
    int raw_member_rank = 0; // VWAP rank with only min trading val filter
    std::string m1_symbol;  // M1 in the group at trade zone entry
    double entry_price = 0;     // actual entry price (元)
    double entry_vwap = 0;      // VWAP at entry (停損基準)
    double day_high_at_entry = 0; // day high at entry (停利基準)
    double prev_close = 0;      // previous close (元)
    double vol_ratio = 0;           // 當日量 / 月均同時段量
    long long month_trading_val = 0; // 月均成交金額
    bool is_prev_day_lu = false;     // 前日漲停
    bool is_disposition = false;     // 處置股
    bool had_circuit_breaker = false; // 曾觸發緩搓
    int group_limit_up_count = 0;    // 進場時族群漲停家數
    double market_entry_chg_pct = 0;  // 0050 at entry time vs prev close (%)
};

class Order {
private:
    std::ofstream logFile;
    std::string logDate; // e.g. "20260211"
    std::string logDir;  // e.g. "./log/20260211_1234/"
    void openLogFile();
    unordered_map<string, std::unique_ptr<std::ofstream>> symbolLogFiles;
    std::ofstream& getSymbolLogFile(const std::string& symbol);
    void closeSymbolLogFiles();
    unordered_map<string, vector<pair<int, double>>> orders;
    
    unordered_map<string, IndexData> entryPointIdx;
    unordered_map<string, SIGNAL_TYPE> entrySignalType;
    
    set<string> stoppedLossSymbols;
    set<string> enteredSymbols;  // all symbols that entered today (never removed)
    std::ofstream tickDumpFile;

    bool stopLoss(format6Type *f6);
    bool timeExit(format6Type *f6);
    bool bailout(format6Type *f6);
    bool takeProfit(format6Type *f6);
    unordered_map<string, bool> profitTaken;

    IniReader reader;
    bool dispostion_enabled = false;
    bool filter_prev_day_limit_up = true;
    double stop_loss_ratio_a = 0.997;
    double stop_loss_ratio_b = 0.997;
    double bailout_ratio = 0.985;
    double max_entry_price = 0;  // 0 = no limit
    long long entry_time_limit = 130'000'000'000;
    long long exit_time_limit = 132'500'000'000;
    int take_profit_splits = 5;
    std::vector<int> take_profit_tick_offsets = {-1, 0, 1, 2, 3};
    std::vector<double> take_profit_pcts;  // percentage-based TP offsets (e.g. 0.01, 0.02, 0.03)
    int reserve_limit_up_splits = 0;       // splits reserved for limit-up
    bool tp_base_entry = true;             // TP base: entry price (true) or day_high (false)
    unordered_map<string, double> reserveStocks;     // reserve qty (not placed as orders)
    unordered_map<string, long long> limitUpPrices;   // limit-up price per symbol
    std::string lastTimeExitCause;                    // "lockedLimitUp" or "timeExit"
    void cancelAll(string symbol);
    void closeAll(string symbol, format6Type *f6);

    // trade tracking for report
    struct OpenTrade {
        std::string symbol;
        std::string signal_type;
        std::string enter_cause;
        long long entry_time_raw = 0;
        double baseline = 0;  // symbolCash before entry
        bool had_take_profit = false;
        std::string group_name;
        int group_rank = 0;
        int member_rank = 0;
        int raw_member_rank = 0;
        std::string m1_symbol;
        double entry_price = 0;
        double entry_vwap = 0;
        double day_high_at_entry = 0;
        double prev_close = 0;
        double vol_ratio = 0;
        long long month_trading_val = 0;
        bool is_prev_day_lu = false;
        bool is_disposition = false;
        bool had_circuit_breaker = false;
        int group_limit_up_count = 0;
        double market_entry_chg_pct = 0;
    };
    unordered_map<string, OpenTrade> openTrades;
    std::vector<TradeRecord> completedTrades;
public:
    unordered_map<string, double> stocks;
    unordered_map<string, double> symbolCash;
    Order();
    ~Order();
    double position = 0; // Interpreted as CASH amount for each entry if using fixed cash position model
    void setDate(const std::string& date, const std::string& logFolder = "");
    void trigger(IndexData &idx, format6Type *f6, int entry_idx, SIGNAL_TYPE signal_type, MatchType matchType, StrongSingle &strongSingle, StrongGroup &strongGroup);
    // void leave(IndexData &idx, format6Type *f6, int entry_idx);
    void on_tick(format6Type *f6);
    bool isStoppedLoss(string symbol) {
        return stoppedLossSymbols.find(symbol) != stoppedLossSymbols.end();
    }

    void clear(string symbol, long long price);
    double cash = 0;
    void generateReport();
    void dumpTick(format6Type *f6);
    QuoteSv *quoteSv = nullptr;
    double market_open_chg_pct = 0;  // 0050 open change %, set by StrategySv
    long long p0050_latest = 0;      // 0050 latest trade price, set by StrategySv
    long long p0050_prev = 0;        // 0050 prev close, set by StrategySv
    long long pending_near_vwap_time = 0;  // set by StrategySv before trigger()
    double pending_near_vwap_pv_ratio = 0; // set by StrategySv before trigger()
};