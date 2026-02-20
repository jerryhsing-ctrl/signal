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

enum class SIGNAL_TYPE {
	SIGNAL_A,
	SIGNAL_B,
    SIGNAL_BOTH
};

class Order {
private:
    std::ofstream logFile;
    std::string logDate; // e.g. "20260211"
    void openLogFile();
    unordered_map<string, std::unique_ptr<std::ofstream>> symbolLogFiles;
    std::ofstream& getSymbolLogFile(const std::string& symbol);
    void closeSymbolLogFiles();
    unordered_map<string, vector<pair<int, double>>> orders;
    
    unordered_map<string, IndexData> entryPointIdx;
    unordered_map<string, SIGNAL_TYPE> entrySignalType;
    
    set<string> stoppedLossSymbols;

    bool stopLoss(format6Type *f6);
    bool timeExit(format6Type *f6);
    bool bailout(format6Type *f6);
    bool takeProfit(format6Type *f6);
    bool marketClose(format6Type *f6);
    unordered_map<string, bool> profitTaken;

    IniReader reader;

    void cancelAll(string symbol);
    void closeAll(string symbol, format6Type *f6);
public:
    unordered_map<string, double> stocks;
    unordered_map<string, double> symbolCash;
    Order();
    ~Order();
    double position = 0; // Interpreted as CASH amount for each entry if using fixed cash position model
    void setDate(const std::string& date);
    void trigger(IndexData &idx, format6Type *f6, int entry_idx, SIGNAL_TYPE signal_type, MatchType matchType, StrongSingle &strongSingle, StrongGroup &strongGroup);
    // void leave(IndexData &idx, format6Type *f6, int entry_idx);
    void on_tick(format6Type *f6);
    bool isStoppedLoss(string symbol) {
        return stoppedLossSymbols.find(symbol) != stoppedLossSymbols.end();
    }

    void clear(string symbol, long long price);
    double cash = 0;
};