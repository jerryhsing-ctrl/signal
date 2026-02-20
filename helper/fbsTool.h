#pragma once
#include "string"
#include "StrategySv/StratProtocol_generated.h"
#include "StrategySv/DeleteTriggerProtocol_generated.h"
#include "timeUtil.h"
#include "errorLog.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

#pragma once

using PARAMS = unordered_map<std::string, std::pair<int, StratProtocol::StrategyParameterT>>;

namespace {
std::string boolText(bool v) {
    return v ? "TRUE" : "FALSE";
}

std::string optString(const flatbuffers::String* s) {
    return s ? s->str() : std::string();
}

std::string sideToString(StratProtocol::enum_side side) {
    switch (side) {
        case StratProtocol::enum_side_Buy: return "Buy";
        case StratProtocol::enum_side_Sell: return "Sell";
        default: return "Unknown";
    }
}

std::string opTypeToString(StratProtocol::enum_opt_type op) {
    switch (op) {
        case StratProtocol::enum_opt_type_AND: return "AND";
        case StratProtocol::enum_opt_type_OR: return "OR";
        default: return "UNKNOWN";
    }
}

std::string tradeAtToString(StratProtocol::enum_trade_at_type tradeAt) {
    switch (tradeAt) {
        case StratProtocol::enum_trade_at_type_Bid: return "Bid(內盤)";
        case StratProtocol::enum_trade_at_type_Ask: return "Ask(外盤)";
        default: return std::to_string(static_cast<int>(tradeAt));
    }
}

std::string priceSourceToString(StratProtocol::enum_price_source_type ps) {
    switch (ps) {
        case StratProtocol::enum_price_source_type_bid: return "bid";
        case StratProtocol::enum_price_source_type_ask: return "ask";
        case StratProtocol::enum_price_source_type_last: return "last";
        default: return std::to_string(static_cast<int>(ps));
    }
}

std::string orderTypeToString(StratProtocol::enum_order_type ot) {
    switch (ot) {
        case StratProtocol::enum_order_type_cash: return "cash";
        case StratProtocol::enum_order_type_margin_purchase: return "margin_purchase";
        case StratProtocol::enum_order_type_short_sale: return "short_sale";
        case StratProtocol::enum_order_type_d_five: return "d_five";
        case StratProtocol::enum_order_type_d_six: return "d_six";
        case StratProtocol::enum_order_type_offset_margin_purchase: return "offset_margin_purchase";
        case StratProtocol::enum_order_type_offset_short_sale: return "offset_short_sale";
        case StratProtocol::enum_order_type_day_trade: return "day_trade";
        default: return std::to_string(static_cast<int>(ot));
    }
}

template <typename EnumT>
int toInt(EnumT e) {
    return static_cast<int>(e);
}
}

// ----------------------------------------------------------------------------
// Update: Added elapsed_time and trigger_time logging
// ----------------------------------------------------------------------------
inline void recordFbsDump(const StratProtocol::StrategyParameter* param, std::ofstream& logFile)
{
    if (!param) {
        errorLog("Null parameter passed to recordFbsDump");
        return;
    }

    std::stringstream ss;

    ss << "==============================================================\n";
    ss << "時間: " << now_hms_nano() << "\n";
    ss << "==============================================================\n";
    ss << "[Received]\n";
    ss << "protocol_type=[" << StratProtocol::EnumNameenum_protocol_type(param->protocol_type()) << "]\n";
    ss << "seqno=[" << optString(param->seqno()) << "]\n";
    ss << "op_type=[" << opTypeToString(param->op_type()) << "]\n";
    ss << "brokerid=[" << optString(param->brokerid()) << "]\n";
    ss << "account=[" << optString(param->account()) << "]\n";
    ss << "symbol=[" << optString(param->symbol()) << "]\n";
    ss << "side=[" << sideToString(param->side()) << "]\n";
    ss << "quantity=[" << param->quantity() << "]\n";
    ss << "ord_prz_type=[" << toInt(param->ord_prz_type()) << "]\n";
    ss << "ord_prz=[" << param->ord_prz() << "]\n";
    ss << "shift_tick=[" << param->shift_tick() << "]\n";
    ss << "order_type=[" << orderTypeToString(param->order_type()) << "]\n";
    ss << "act_no_new=[" << boolText(param->act_no_new()) << "]\n";
    ss << "act_del_bid=[" << boolText(param->act_del_bid()) << "]\n";
    ss << "act_del_ask=[" << boolText(param->act_del_ask()) << "]\n";
    ss << "del_bid_type=[" << toInt(param->del_bid_type()) << "]\n";
    ss << "del_ask_type=[" << toInt(param->del_ask_type()) << "]\n";
    ss << "status=[" << StratProtocol::EnumNameenum_status(param->status()) << "]\n";
    ss << "valid=[" << boolText(param->valid()) << "]\n";
    ss << "msg=[" << optString(param->msg()) << "]\n";
    // [Updated]
    ss << "elapsed_time=[" << param->elapsed_time() << "]\n";
    ss << "trigger_time=[" << param->trigger_time() << "]\n";

    auto dumpCond = [&ss](int idx, const char* name, const auto* cond) {
        ss << idx << ". " << name << "\n";
        ss << "   check=[" << boolText(cond ? cond->check() : false) << "]\n";
    };

    const auto* c1 = param->cond1();
    dumpCond(1, "cond1 (委賣五檔總量)", c1);
    ss << "   total_ask_value=[" << (c1 ? c1->total_ask_value() : 0) << "]\n";
    ss << "   total_vol=[" << (c1 ? c1->total_vol() : 0) << "]\n";
    ss << "   price_source=[" << (c1 ? priceSourceToString(c1->price_source()) : std::string("None")) << "]\n";
    ss << "   order_price=[" << (c1 ? c1->order_price() : 0) << "]\n";
    ss << "   shift_tick=[" << (c1 ? c1->shift_tick() : 0) << "]\n";

    const auto* c2 = param->cond2();
    dumpCond(2, "cond2 (累積量)", c2);
    ss << "   time_cond=[" << (c2 ? c2->time_cond() : 0) << "]\n";
    ss << "   trade_at=[" << (c2 ? tradeAtToString(c2->trade_at()) : std::string("None")) << "]\n";
    ss << "   sum_vol=[" << (c2 ? c2->sum_vol() : 0) << "]\n";
    ss << "   order_price=[" << (c2 ? c2->order_price() : 0) << "]\n";
    ss << "   shift_tick=[" << (c2 ? c2->shift_tick() : 0) << "]\n";

    const auto* c3 = param->cond3();
    dumpCond(3, "cond3 (漲停鎖死)", c3);

    const auto* c4 = param->cond4();
    dumpCond(4, "cond4 (成交價/買賣價)", c4);
    ss << "   price_source=[" << (c4 ? priceSourceToString(c4->price_source()) : std::string("None")) << "]\n";
    ss << "   compare_type=[" << (c4 ? toInt(c4->compare_type()) : 0) << "]\n";
    ss << "   ord_prz=[" << (c4 ? c4->ord_prz() : 0) << "]\n";
    ss << "   shift_tick=[" << (c4 ? c4->shift_tick() : 0) << "]\n";

    const auto* c5 = param->cond5();
    dumpCond(5, "cond5 (單量)", c5);
    ss << "   trade_at=[" << (c5 ? tradeAtToString(c5->trade_at()) : std::string("None")) << "]\n";
    ss << "   compare_type=[" << (c5 ? toInt(c5->compare_type()) : 0) << "]\n";
    ss << "   volume=[" << (c5 ? c5->volume() : 0) << "]\n";

    const auto* c6 = param->cond6();
    dumpCond(6, "cond6 (漲停市價委買量小於)", c6);
    ss << "   volume=[" << (c6 ? c6->volume() : 0) << "]\n";

    const auto* c7 = param->cond7();
    dumpCond(7, "cond7 (跌停鎖死)", c7);

    const auto* c8 = param->cond8();
    dumpCond(8, "cond8 (買賣退縮)", c8);
    ss << "   volume=[" << (c8 ? c8->volume() : 0) << "]\n";
    ss << "   order_price=[" << (c8 ? c8->order_price() : 0) << "]\n";
    ss << "   shift_tick=[" << (c8 ? c8->shift_tick() : 0) << "]\n";

    ss << "--------------------------------------------------------------\n\n";

    std::string logContent = ss.str();
    if (logFile.is_open()) {
        logFile << logContent;
        logFile.flush();
    }
    else {
        errorLog("logFile is not open!");
    }
}

// ----------------------------------------------------------------------------
// Update: Added elapsed_time and trigger_time logging
// ----------------------------------------------------------------------------
inline void logStrategyParam(const StratProtocol::StrategyParameterT& param, std::ofstream& logFile) {
    std::stringstream ss;

    ss << "==============================================================\n";
    ss << "時間: " << now_hms_nano() << "\n";
    ss << "==============================================================\n";
    ss << "[Log]\n";
    ss << "protocol_type=[" << StratProtocol::EnumNameenum_protocol_type(param.protocol_type) << "]\n";
    ss << "seqno=[" << param.seqno << "]\n";
    ss << "op_type=[" << opTypeToString(param.op_type) << "]\n";
    ss << "brokerid=[" << param.brokerid << "]\n";
    ss << "account=[" << param.account << "]\n";
    ss << "symbol=[" << param.symbol << "]\n";
    ss << "side=[" << sideToString(param.side) << "]\n";
    ss << "quantity=[" << param.quantity << "]\n";
    ss << "ord_prz_type=[" << toInt(param.ord_prz_type) << "]\n";
    ss << "ord_prz=[" << param.ord_prz << "]\n";
    ss << "shift_tick=[" << param.shift_tick << "]\n";
    ss << "order_type=[" << orderTypeToString(param.order_type) << "]\n";
    ss << "act_no_new=[" << boolText(param.act_no_new) << "]\n";
    ss << "act_del_bid=[" << boolText(param.act_del_bid) << "]\n";
    ss << "act_del_ask=[" << boolText(param.act_del_ask) << "]\n";
    ss << "del_bid_type=[" << toInt(param.del_bid_type) << "]\n";
    ss << "del_ask_type=[" << toInt(param.del_ask_type) << "]\n";
    ss << "status=[" << StratProtocol::EnumNameenum_status(param.status) << "]\n";
    ss << "valid=[" << boolText(param.valid) << "]\n";
    ss << "msg=[" << param.msg << "]\n";
    // [Updated]
    ss << "elapsed_time=[" << param.elapsed_time << "]\n";
    ss << "trigger_time=[" << param.trigger_time << "]\n";

    auto dumpCond = [&ss](int idx, const char* name, const auto& cond) {
        ss << idx << ". " << name << "\n";
        ss << "   check=[" << boolText(cond ? cond->check : false) << "]\n";
    };

    const auto& c1 = param.cond1;
    dumpCond(1, "cond1 (委賣五檔總量)", c1);
    ss << "   total_ask_value=[" << (c1 ? c1->total_ask_value : 0) << "]\n";
    ss << "   total_vol=[" << (c1 ? c1->total_vol : 0) << "]\n";
    ss << "   price_source=[" << (c1 ? priceSourceToString(c1->price_source) : std::string("None")) << "]\n";
    ss << "   order_price=[" << (c1 ? c1->order_price : 0) << "]\n";
    ss << "   shift_tick=[" << (c1 ? c1->shift_tick : 0) << "]\n";


    // ss << "   total_ask_value=[" << (c1 ? c1->total_ask_value() : 0) << "]\n";
    // ss << "   total_vol=[" << (c1 ? c1->total_vol() : 0) << "]\n";
    // ss << "   price_source=[" << (c1 ? priceSourceToString(c1->price_source()) : std::string("None")) << "]\n";
    // ss << "   order_price=[" << (c1 ? c1->order_price() : 0) << "]\n";
    // ss << "   shift_tick=[" << (c1 ? c1->shift_tick() : 0) << "]\n";

    const auto& c2 = param.cond2;
    dumpCond(2, "cond2 (累積量)", c2);
    ss << "   time_cond=[" << (c2 ? c2->time_cond : 0) << "]\n";
    ss << "   trade_at=[" << (c2 ? tradeAtToString(c2->trade_at) : std::string("None")) << "]\n";
    ss << "   sum_vol=[" << (c2 ? c2->sum_vol : 0) << "]\n";
    ss << "   order_price=[" << (c2 ? c2->order_price : 0) << "]\n";
    ss << "   shift_tick=[" << (c2 ? c2->shift_tick : 0) << "]\n";

    const auto& c3 = param.cond3;
    dumpCond(3, "cond3 (漲停鎖死)", c3);

    const auto& c4 = param.cond4;
    dumpCond(4, "cond4 (成交價/買賣價)", c4);
    ss << "   price_source=[" << (c4 ? priceSourceToString(c4->price_source) : std::string("None")) << "]\n";
    ss << "   compare_type=[" << (c4 ? toInt(c4->compare_type) : 0) << "]\n";
    ss << "   ord_prz=[" << (c4 ? c4->ord_prz : 0) << "]\n";
    ss << "   shift_tick=[" << (c4 ? c4->shift_tick : 0) << "]\n";

    const auto& c5 = param.cond5;
    dumpCond(5, "cond5 (單量)", c5);
    ss << "   trade_at=[" << (c5 ? tradeAtToString(c5->trade_at) : std::string("None")) << "]\n";
    ss << "   compare_type=[" << (c5 ? toInt(c5->compare_type) : 0) << "]\n";
    ss << "   volume=[" << (c5 ? c5->volume : 0) << "]\n";

    const auto& c6 = param.cond6;
    dumpCond(6, "cond6 (漲停市價委買量小於)", c6);
    ss << "   volume=[" << (c6 ? c6->volume : 0) << "]\n";

    const auto& c7 = param.cond7;
    dumpCond(7, "cond7 (跌停鎖死)", c7);

    const auto& c8 = param.cond8;
    dumpCond(8, "cond8 (買賣退縮)", c8);
    ss << "   volume=[" << (c8 ? c8->volume : 0) << "]\n";
    ss << "   order_price=[" << (c8 ? c8->order_price : 0) << "]\n";
    ss << "   shift_tick=[" << (c8 ? c8->shift_tick : 0) << "]\n";

    ss << "--------------------------------------------------------------\n\n";
    
    std::string logContent = ss.str();
    if (logFile.is_open()) {
        logFile << logContent;
        logFile.flush();
    }
    else {
        errorLog("logFile is not open!");
    }
}

// ----------------------------------------------------------------------------
// Update: Added elapsed_time and trigger_time to CSV output
// ----------------------------------------------------------------------------
inline void paramToCsv(const StratProtocol::StrategyParameterT& param, std::ofstream& outFile){
    std::stringstream ss;

    ss << now_hms_nano() << ",";
    ss << StratProtocol::EnumNameenum_protocol_type(param.protocol_type) << ",";
    ss << param.seqno << ",";
    ss << opTypeToString(param.op_type) << ",";
    ss << param.brokerid << ",";
    ss << param.account << ",";
    ss << param.symbol << ",";
    ss << sideToString(param.side) << ",";
    ss << param.quantity << ",";
    ss << toInt(param.ord_prz_type) << ",";
    ss << param.ord_prz << ",";
    ss << param.shift_tick << ",";
    ss << orderTypeToString(param.order_type) << ",";
    ss << boolText(param.act_no_new) << ",";
    ss << boolText(param.act_del_bid) << ",";
    ss << boolText(param.act_del_ask) << ",";
    ss << toInt(param.del_bid_type) << ",";
    ss << toInt(param.del_ask_type) << ",";
    ss << StratProtocol::EnumNameenum_status(param.status) << ",";
    ss << boolText(param.valid) << ",";
    ss << "\"" << param.msg << "\",";
    // [Updated] Inserted here to maintain scalar grouping
    ss << param.elapsed_time << ",";
    ss << param.trigger_time << ",";

    const auto& c1 = param.cond1;
    ss << (c1 ? boolText(c1->check) : "FALSE") << ",";
    ss << (c1 ? c1->total_ask_value : 0) << ",";
    ss << (c1 ? c1->total_vol : 0) << ",";
    ss << (c1 ? priceSourceToString(c1->price_source) : "None") << ",";
    ss << (c1 ? c1->order_price : 0) << ",";
    ss << (c1 ? c1->shift_tick : 0) << ",";

    

    const auto& c2 = param.cond2;
    ss << (c2 ? boolText(c2->check) : "FALSE") << ",";
    ss << (c2 ? c2->time_cond : 0) << ",";
    ss << (c2 ? tradeAtToString(c2->trade_at) : "None") << ",";
    ss << (c2 ? c2->sum_vol : 0) << ",";
    ss << (c2 ? c2->order_price : 0) << ",";
    ss << (c2 ? c2->shift_tick : 0) << ",";

    const auto& c3 = param.cond3;
    ss << (c3 ? boolText(c3->check) : "FALSE") << ",";

    const auto& c4 = param.cond4;
    ss << (c4 ? boolText(c4->check) : "FALSE") << ",";
    ss << (c4 ? priceSourceToString(c4->price_source) : "None") << ",";
    ss << (c4 ? toInt(c4->compare_type) : 0) << ",";
    ss << (c4 ? c4->ord_prz : 0) << ",";
    ss << (c4 ? c4->shift_tick : 0) << ",";

    const auto& c5 = param.cond5;
    ss << (c5 ? boolText(c5->check) : "FALSE") << ",";
    ss << (c5 ? tradeAtToString(c5->trade_at) : "None") << ",";
    ss << (c5 ? toInt(c5->compare_type) : 0) << ",";
    ss << (c5 ? c5->volume : 0) << ",";

    const auto& c6 = param.cond6;
    ss << (c6 ? boolText(c6->check) : "FALSE") << ",";
    ss << (c6 ? c6->volume : 0) << ",";

    const auto& c7 = param.cond7;
    ss << (c7 ? boolText(c7->check) : "FALSE") << ",";

    const auto& c8 = param.cond8;
    ss << (c8 ? boolText(c8->check) : "FALSE") << ",";
    ss << (c8 ? c8->volume : 0) << ",";
    ss << (c8 ? c8->order_price : 0) << ",";
    ss << (c8 ? c8->shift_tick : 0);

    ss << "\n";

    std::string logContent = ss.str();
    if (outFile.is_open()) {
        outFile << logContent;
        outFile.flush();
    }
    else {
        errorLog("logFile is not open!");
    }
}

static bool textToBool(const std::string& s) {
    return s == "TRUE";
}

template<typename EnumT>
static EnumT stringToEnum(const std::string& s, const char* const* names, EnumT maxVal) {
    for (int i = 0; names[i] != nullptr; ++i) {
        if (s == names[i]) return static_cast<EnumT>(i);
    }
    return static_cast<EnumT>(0);
}

static StratProtocol::enum_trade_at_type stringToTradeAt(const std::string& s) {
    if (s == "Bid(內盤)") return StratProtocol::enum_trade_at_type_Bid;
    if (s == "Ask(外盤)") return StratProtocol::enum_trade_at_type_Ask;
    return StratProtocol::enum_trade_at_type_Bid;
}

static StratProtocol::enum_price_source_type stringToPriceSource(const std::string& s) {
    if (s == "bid") return StratProtocol::enum_price_source_type_bid;
    if (s == "ask") return StratProtocol::enum_price_source_type_ask;
    if (s == "last") return StratProtocol::enum_price_source_type_last;
    return StratProtocol::enum_price_source_type_bid;
}

static StratProtocol::enum_order_type stringToOrderType(const std::string& s) {
    if (s == "cash") return StratProtocol::enum_order_type_cash;
    if (s == "margin_purchase") return StratProtocol::enum_order_type_margin_purchase;
    if (s == "short_sale") return StratProtocol::enum_order_type_short_sale;
    if (s == "d_five") return StratProtocol::enum_order_type_d_five;
    if (s == "d_six") return StratProtocol::enum_order_type_d_six;
    if (s == "offset_margin_purchase") return StratProtocol::enum_order_type_offset_margin_purchase;
    if (s == "offset_short_sale") return StratProtocol::enum_order_type_offset_short_sale;
    if (s == "day_trade") return StratProtocol::enum_order_type_day_trade;
    return StratProtocol::enum_order_type_cash;
}

// ----------------------------------------------------------------------------
// Update: Added parsing for elapsed_time and trigger_time
// ----------------------------------------------------------------------------
inline void CsvToParamMap(std::ifstream& inFile, SharedMap<std::string, pair<int, StratProtocol::StrategyParameterT>>& paramMap){
    if (!inFile.is_open()) {
        // errorLog("Failed to open %s for reading.", LOG_STRATEGY_PARAM_MAP);
        return;
    }

    std::string line;
    while (std::getline(inFile, line)) {
        if (line.empty()) continue;
        cout << " Reading line: " << line << "\n";

        std::vector<std::string> tokens;
        bool inQuote = false;
        std::string current;
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') {
                inQuote = !inQuote;
            } else if (c == ',' && !inQuote) {
                tokens.push_back(current);
                current.clear();
            } else {
                current += c;
            }
        }
        tokens.push_back(current);

        // [Updated] Minimum tokens increased from 47 to 49 due to 2 new fields
        // if (tokens.size() < 49) {
        //     errorLog("Invalid CSV line (not enough tokens): " + std::to_string(tokens.size()));
        //     continue;
        // }
        
        try {
            StratProtocol::StrategyParameterT p;
            int idx = 1; 

            p.protocol_type = stringToEnum(tokens[idx++], StratProtocol::EnumNamesenum_protocol_type(), StratProtocol::enum_protocol_type_MAX);
            p.seqno = tokens[idx++];
            p.op_type = stringToEnum(tokens[idx++], StratProtocol::EnumNamesenum_opt_type(), StratProtocol::enum_opt_type_MAX);
            p.brokerid = tokens[idx++];
            p.account = tokens[idx++];
            p.symbol = tokens[idx++];
            p.side = stringToEnum(tokens[idx++], StratProtocol::EnumNamesenum_side(), StratProtocol::enum_side_MAX);
            p.quantity = std::stoi(tokens[idx++]);
            p.ord_prz_type = static_cast<StratProtocol::enum_ord_prz_type>(std::stoi(tokens[idx++]));
            p.ord_prz = std::stoll(tokens[idx++]);
            p.shift_tick = std::stoi(tokens[idx++]);
            p.order_type = stringToOrderType(tokens[idx++]);
            p.act_no_new = textToBool(tokens[idx++]);
            p.act_del_bid = textToBool(tokens[idx++]);
            p.act_del_ask = textToBool(tokens[idx++]);
            p.del_bid_type = static_cast<StratProtocol::enum_del_type>(std::stoi(tokens[idx++]));
            p.del_ask_type = static_cast<StratProtocol::enum_del_type>(std::stoi(tokens[idx++]));
            p.status = stringToEnum(tokens[idx++], StratProtocol::EnumNamesenum_status(), StratProtocol::enum_status_MAX);
            p.valid = textToBool(tokens[idx++]);
            p.msg = tokens[idx++]; 

            // [Updated] Read new fields
            p.elapsed_time = std::stoll(tokens[idx++]);
            p.trigger_time = std::stoll(tokens[idx++]);

            p.cond1 = std::make_unique<StratProtocol::Condition1T>();
            p.cond1->check = textToBool(tokens[idx++]);
            p.cond1->total_ask_value = std::stoll(tokens[idx++]);
            p.cond1->total_vol = std::stoll(tokens[idx++]);
            p.cond1->price_source = stringToPriceSource(tokens[idx++]);
            p.cond1->order_price = std::stoll(tokens[idx++]);
            p.cond1->shift_tick = std::stoi(tokens[idx++]);

            p.cond2 = std::make_unique<StratProtocol::Condition2T>();
            p.cond2->check = textToBool(tokens[idx++]);
            p.cond2->time_cond = std::stoi(tokens[idx++]);
            p.cond2->trade_at = stringToTradeAt(tokens[idx++]);
            p.cond2->sum_vol = std::stoll(tokens[idx++]);
            p.cond2->order_price = std::stoll(tokens[idx++]);
            p.cond2->shift_tick = std::stoi(tokens[idx++]);

            p.cond3 = std::make_unique<StratProtocol::Condition3T>();
            p.cond3->check = textToBool(tokens[idx++]);

            p.cond4 = std::make_unique<StratProtocol::Condition4T>();
            p.cond4->check = textToBool(tokens[idx++]);
            p.cond4->price_source = stringToPriceSource(tokens[idx++]);
            p.cond4->compare_type = static_cast<StratProtocol::enum_compare_type>(std::stoi(tokens[idx++]));
            p.cond4->ord_prz = std::stoll(tokens[idx++]);
            p.cond4->shift_tick = std::stoi(tokens[idx++]);

            p.cond5 = std::make_unique<StratProtocol::Condition5T>();
            p.cond5->check = textToBool(tokens[idx++]);
            p.cond5->trade_at = stringToTradeAt(tokens[idx++]);
            p.cond5->compare_type = static_cast<StratProtocol::enum_compare_type>(std::stoi(tokens[idx++]));
            p.cond5->volume = std::stoll(tokens[idx++]);

            p.cond6 = std::make_unique<StratProtocol::Condition6T>();
            p.cond6->check = textToBool(tokens[idx++]);
            p.cond6->volume = std::stoll(tokens[idx++]);

            p.cond7 = std::make_unique<StratProtocol::Condition7T>();
            p.cond7->check = textToBool(tokens[idx++]);

            p.cond8 = std::make_unique<StratProtocol::Condition8T>();
            p.cond8->check = textToBool(tokens[idx++]);
            p.cond8->volume = std::stoll(tokens[idx++]);
            p.cond8->order_price = std::stoll(tokens[idx++]);
            p.cond8->shift_tick = std::stoi(tokens[idx++]);

            std::string key = p.seqno;
            // paramMap[key] = {0, std::move(p)};
            paramMap.put(key, {0, p});
            // paramMap[p.seqno] = {0, std::move(p)};
            // cout << " Loaded param for seqno: [" << p.seqno << "]\n";
        } catch (const std::exception& e) {
            errorLog("Error parsing CSV line: " + line + " Error: " + e.what());
        }
    }
}

// ============================================================================
// DeleteTriggerProtocol helpers (mirror StratProtocol helpers)
// ============================================================================
namespace {
inline std::string delTradeAtToString(DeleteTriggerProtocol::enum_trade_at_type tradeAt) {
    switch (tradeAt) {
        case DeleteTriggerProtocol::enum_trade_at_type_Bid: return "Bid(內盤)";
        case DeleteTriggerProtocol::enum_trade_at_type_Ask: return "Ask(外盤)";
        default: return std::to_string(static_cast<int>(tradeAt));
    }
}
inline std::string delCompareToString(DeleteTriggerProtocol::enum_compare_type cmp) {
    switch (cmp) {
        case DeleteTriggerProtocol::enum_compare_type_GE: return "GE";
        case DeleteTriggerProtocol::enum_compare_type_LE: return "LE";
        default: return std::to_string(static_cast<int>(cmp));
    }
}
}

// ----------------------------------------------------------------------------
// recordFbsDump for DeleteTriggerProtocol
// ----------------------------------------------------------------------------
inline void recordFbsDump(const DeleteTriggerProtocol::DeleteTriggerParameter* param, std::ofstream& logFile)
{
    if (!param) {
        errorLog("Null parameter passed to recordFbsDump(DeleteTrigger)");
        return;
    }

    std::stringstream ss;
    ss << "==============================================================\n";
    ss << "時間: " << now_hms_nano() << "\n";
    ss << "==============================================================\n";
    ss << "[Received-Del]" << "\n";
    ss << "protocol_type=[" << DeleteTriggerProtocol::EnumNameenum_protocol_type(param->protocol_type()) << "]\n";
    ss << "seqno=[" << optString(param->seqno()) << "]\n";
    ss << "brokerid=[" << optString(param->brokerid()) << "]\n";
    ss << "account=[" << optString(param->account()) << "]\n";
    ss << "symbol=[" << optString(param->symbol()) << "]\n";
    ss << "action=[" << DeleteTriggerProtocol::EnumNameenum_delete_action_type(param->action()) << "]\n";
    ss << "action_price=[" << param->action_price() << "]\n";
    ss << "status=[" << DeleteTriggerProtocol::EnumNameenum_status(param->status()) << "]\n";
    ss << "valid=[" << boolText(param->valid()) << "]\n";
    ss << "msg=[" << optString(param->msg()) << "]\n";
    ss << "elapsed_time=[" << param->elapsed_time() << "]\n";
    ss << "trigger_time=[" << param->trigger_time() << "]\n";

    // 與 StratProtocol 相同：以 1~8 編號列出各條件，並加上條件名稱
    auto dumpCondN = [&ss](int idx, const char* name, const auto* cond){
        ss << idx << ". " << name << "\n";
        ss << "   check=[" << boolText(cond ? cond->check() : false) << "]\n";
    };

    const auto* c1 = param->cond_volume_in_seconds();
    dumpCondN(1, "cond_volume_in_seconds (N 秒內成交量累計 >= X 張)", c1);
    ss << "   time_ms=[" << (c1 ? c1->time_ms() : 0) << "]\n";
    ss << "   volume=[" << (c1 ? c1->volume() : 0) << "]\n";

    const auto* c2 = param->cond_single_trade_volume();
    dumpCondN(2, "cond_single_trade_volume (單量 >= X 張)", c2);
    ss << "   volume=[" << (c2 ? c2->volume() : 0) << "]\n";

    const auto* c3 = param->cond_best_bid_ask_volume();
    dumpCondN(3, "cond_best_bid_ask_volume (買一量 / 賣一量 >= / <= X 張)", c3);
    ss << "   trade_at=[" << (c3 ? delTradeAtToString(c3->trade_at()) : std::string("None")) << "]\n";
    ss << "   compare_type=[" << (c3 ? delCompareToString(c3->compare_type()) : std::string("None")) << "]\n";
    ss << "   volume=[" << (c3 ? c3->volume() : 0) << "]\n";

    const auto* c4 = param->cond_total_bid_volume();
    dumpCondN(4, "cond_total_bid_volume (委買量 <= X 張)", c4);
    ss << "   volume=[" << (c4 ? c4->volume() : 0) << "]\n";

    const auto* c5 = param->cond_total_ask_volume();
    dumpCondN(5, "cond_total_ask_volume (委賣量 <= X 張)", c5);
    ss << "   volume=[" << (c5 ? c5->volume() : 0) << "]\n";

    const auto* c6 = param->cond_last_price_greater_equal();
    dumpCondN(6, "cond_last_price_greater_equal (成交價 >= P)", c6);
    ss << "   price=[" << (c6 ? c6->price() : 0) << "]\n";

    const auto* c7 = param->cond_last_price_less_equal();
    dumpCondN(7, "cond_last_price_less_equal (成交價 <= P)", c7);
    ss << "   price=[" << (c7 ? c7->price() : 0) << "]\n";

    const auto* c8 = param->cond_trigger_at_time();
    dumpCondN(8, "cond_trigger_at_time (觸時刪單，指定時間點，單位：當日第幾秒)", c8);
    ss << "   time_sec=[" << (c8 ? c8->time_sec() : 0) << "]\n";

    ss << "--------------------------------------------------------------\n\n";

    std::string logContent = ss.str();
    if (logFile.is_open()) { logFile << logContent; logFile.flush(); }
    else { errorLog("logFile is not open!"); }
}

// ----------------------------------------------------------------------------
// logStrategyParam overload for DeleteTriggerProtocol
// ----------------------------------------------------------------------------
inline void logStrategyParam(const DeleteTriggerProtocol::DeleteTriggerParameterT& param, std::ofstream& logFile) {
    std::stringstream ss;

    ss << "==============================================================\n";
    ss << "時間: " << now_hms_nano() << "\n";
    ss << "==============================================================\n";
    ss << "[Log-Del]\n";
    ss << "protocol_type=[" << DeleteTriggerProtocol::EnumNameenum_protocol_type(param.protocol_type) << "]\n";
    ss << "seqno=[" << param.seqno << "]\n";
    ss << "brokerid=[" << param.brokerid << "]\n";
    ss << "account=[" << param.account << "]\n";
    ss << "symbol=[" << param.symbol << "]\n";
    ss << "action=[" << DeleteTriggerProtocol::EnumNameenum_delete_action_type(param.action) << "]\n";
    ss << "action_price=[" << param.action_price << "]\n";
    ss << "status=[" << DeleteTriggerProtocol::EnumNameenum_status(param.status) << "]\n";
    ss << "valid=[" << boolText(param.valid) << "]\n";
    ss << "msg=[" << param.msg << "]\n";
    ss << "elapsed_time=[" << param.elapsed_time << "]\n";
    ss << "trigger_time=[" << param.trigger_time << "]\n";

    auto dumpCond = [&ss](int idx, const char* name, const auto& cond){
        ss << idx << ". " << name << "\n";
        ss << "   check=[" << boolText(cond ? cond->check : false) << "]\n";
    };

    int idx = 1;
    const auto& c1 = param.cond_volume_in_seconds; dumpCond(idx++, "cond_volume_in_seconds (N 秒內成交量累計 >= X 張)", c1);
    ss << "   time_ms=[" << (c1 ? c1->time_ms : 0) << "]\n";
    ss << "   volume=[" << (c1 ? c1->volume : 0) << "]\n";

    const auto& c2 = param.cond_single_trade_volume; dumpCond(idx++, "cond_single_trade_volume (單量 >= X 張)", c2);
    ss << "   volume=[" << (c2 ? c2->volume : 0) << "]\n";

    const auto& c3 = param.cond_best_bid_ask_volume; dumpCond(idx++, "cond_best_bid_ask_volume (買一量 / 賣一量 >= / <= X 張)", c3);
    ss << "   trade_at=[" << (c3 ? delTradeAtToString(c3->trade_at) : std::string("None")) << "]\n";
    ss << "   compare_type=[" << (c3 ? delCompareToString(c3->compare_type) : std::string("None")) << "]\n";
    ss << "   volume=[" << (c3 ? c3->volume : 0) << "]\n";

    const auto& c4 = param.cond_total_bid_volume; dumpCond(idx++, "cond_total_bid_volume (委買量 <= X 張)", c4);
    ss << "   volume=[" << (c4 ? c4->volume : 0) << "]\n";

    const auto& c5 = param.cond_total_ask_volume; dumpCond(idx++, "cond_total_ask_volume (委賣量 <= X 張)", c5);
    ss << "   volume=[" << (c5 ? c5->volume : 0) << "]\n";

    const auto& c6 = param.cond_last_price_greater_equal; dumpCond(idx++, "cond_last_price_greater_equal (成交價 >= P)", c6);
    ss << "   price=[" << (c6 ? c6->price : 0) << "]\n";

    const auto& c7 = param.cond_last_price_less_equal; dumpCond(idx++, "cond_last_price_less_equal (成交價 <= P)", c7);
    ss << "   price=[" << (c7 ? c7->price : 0) << "]\n";

    const auto& c8 = param.cond_trigger_at_time; dumpCond(idx++, "cond_trigger_at_time (觸時刪單，指定時間點，單位：當日第幾秒)", c8);
    ss << "   time_sec=[" << (c8 ? c8->time_sec : 0) << "]\n";

    ss << "--------------------------------------------------------------\n\n";

    std::string logContent = ss.str();
    if (logFile.is_open()) { logFile << logContent; logFile.flush(); }
    else { errorLog("logFile is not open!"); }
}

// ----------------------------------------------------------------------------
// paramToCsv overload for DeleteTriggerProtocol
// ----------------------------------------------------------------------------
inline void paramToCsv(const DeleteTriggerProtocol::DeleteTriggerParameterT& param, std::ofstream& outFile){
    std::stringstream ss;
    ss << now_hms_nano() << ",";
    ss << DeleteTriggerProtocol::EnumNameenum_protocol_type(param.protocol_type) << ",";
    ss << param.seqno << ",";
    ss << param.brokerid << ",";
    ss << param.account << ",";
    ss << param.symbol << ",";
    ss << DeleteTriggerProtocol::EnumNameenum_delete_action_type(param.action) << ",";
    ss << param.action_price << ",";
    ss << DeleteTriggerProtocol::EnumNameenum_status(param.status) << ",";
    ss << boolText(param.valid) << ",";
    ss << "\"" << param.msg << "\"" << ",";
    ss << param.elapsed_time << ",";
    ss << param.trigger_time << ",";

    const auto& c1 = param.cond_volume_in_seconds;
    ss << (c1 ? boolText(c1->check) : "FALSE") << ",";
    ss << (c1 ? c1->time_ms : 0) << ",";
    ss << (c1 ? c1->volume : 0) << ",";

    const auto& c2 = param.cond_single_trade_volume;
    ss << (c2 ? boolText(c2->check) : "FALSE") << ",";
    ss << (c2 ? c2->volume : 0) << ",";

    const auto& c3 = param.cond_best_bid_ask_volume;
    ss << (c3 ? boolText(c3->check) : "FALSE") << ",";
    ss << (c3 ? delTradeAtToString(c3->trade_at) : "None") << ",";
    ss << (c3 ? delCompareToString(c3->compare_type) : "None") << ",";
    ss << (c3 ? c3->volume : 0) << ",";

    const auto& c4 = param.cond_total_bid_volume;
    ss << (c4 ? boolText(c4->check) : "FALSE") << ",";
    ss << (c4 ? c4->volume : 0) << ",";

    const auto& c5 = param.cond_total_ask_volume;
    ss << (c5 ? boolText(c5->check) : "FALSE") << ",";
    ss << (c5 ? c5->volume : 0) << ",";

    const auto& c6 = param.cond_last_price_greater_equal;
    ss << (c6 ? boolText(c6->check) : "FALSE") << ",";
    ss << (c6 ? c6->price : 0) << ",";

    const auto& c7 = param.cond_last_price_less_equal;
    ss << (c7 ? boolText(c7->check) : "FALSE") << ",";
    ss << (c7 ? c7->price : 0) << ",";

    const auto& c8 = param.cond_trigger_at_time;
    ss << (c8 ? boolText(c8->check) : "FALSE") << ",";
    ss << (c8 ? c8->time_sec : 0);

    ss << "\n";
    std::string logContent = ss.str();
    if (outFile.is_open()) { outFile << logContent; outFile.flush(); }
    else { errorLog("logFile is not open!"); }
}

// ----------------------------------------------------------------------------
// CsvToParamMap overload for DeleteTriggerProtocol
// ----------------------------------------------------------------------------
inline void CsvToParamMap(std::ifstream& inFile, SharedMap<std::string, pair<int, DeleteTriggerProtocol::DeleteTriggerParameterT>>& paramMap){
    if (!inFile.is_open()) { return; }
    std::string line;
    while (std::getline(inFile, line)) {
        if (line.empty()) continue;

        std::vector<std::string> tokens;
        bool inQuote = false; std::string current;
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') inQuote = !inQuote;
            else if (c == ',' && !inQuote) { tokens.push_back(current); current.clear(); }
            else current += c;
        }
        tokens.push_back(current);

        // fields count: timestamp + 12 base + conditions
        // timestamp, protocol, seqno, brokerid, account, symbol, action, action_price, status, valid, msg, elapsed_time, trigger_time,
        // vis(check,time_ms,volume)=3, stv(check,volume)=2, bbav(check,trade_at,compare,volume)=4, tbv(check,volume)=2, tav(check,volume)=2,
        // lge(check,price)=2, lle(check,price)=2, tat(check,time_sec)=2
        // total minimal = 1 + 12 + (3+2+4+2+2+2+2+2) = 32
        if (tokens.size() < 32) { errorLog("Invalid CSV line (DeleteTrigger): not enough tokens: " + std::to_string(tokens.size())); continue; }

        try {
            DeleteTriggerProtocol::DeleteTriggerParameterT p;
            int idx = 1; // skip timestamp
            p.protocol_type = stringToEnum(tokens[idx++], DeleteTriggerProtocol::EnumNamesenum_protocol_type(), DeleteTriggerProtocol::enum_protocol_type_MAX);
            p.seqno = tokens[idx++];
            p.brokerid = tokens[idx++];
            p.account = tokens[idx++];
            p.symbol = tokens[idx++];
            p.action = stringToEnum(tokens[idx++], DeleteTriggerProtocol::EnumNamesenum_delete_action_type(), DeleteTriggerProtocol::enum_delete_action_type_MAX);
            p.action_price = std::stoll(tokens[idx++]);
            p.status = stringToEnum(tokens[idx++], DeleteTriggerProtocol::EnumNamesenum_status(), DeleteTriggerProtocol::enum_status_MAX);
            p.valid = textToBool(tokens[idx++]);
            p.msg = tokens[idx++];
            p.elapsed_time = std::stoll(tokens[idx++]);
            p.trigger_time = std::stoll(tokens[idx++]);

            p.cond_volume_in_seconds = std::make_unique<DeleteTriggerProtocol::VolumeInSecondsT>();
            p.cond_volume_in_seconds->check = textToBool(tokens[idx++]);
            p.cond_volume_in_seconds->time_ms = std::stoi(tokens[idx++]);
            p.cond_volume_in_seconds->volume = std::stoll(tokens[idx++]);

            p.cond_single_trade_volume = std::make_unique<DeleteTriggerProtocol::SingleTradeVolumeT>();
            p.cond_single_trade_volume->check = textToBool(tokens[idx++]);
            p.cond_single_trade_volume->volume = std::stoll(tokens[idx++]);

            p.cond_best_bid_ask_volume = std::make_unique<DeleteTriggerProtocol::BestBidAskVolumeT>();
            p.cond_best_bid_ask_volume->check = textToBool(tokens[idx++]);
            p.cond_best_bid_ask_volume->trade_at = stringToEnum(tokens[idx++], DeleteTriggerProtocol::EnumNamesenum_trade_at_type(), DeleteTriggerProtocol::enum_trade_at_type_MAX);
            p.cond_best_bid_ask_volume->compare_type = stringToEnum(tokens[idx++], DeleteTriggerProtocol::EnumNamesenum_compare_type(), DeleteTriggerProtocol::enum_compare_type_MAX);
            p.cond_best_bid_ask_volume->volume = std::stoll(tokens[idx++]);

            p.cond_total_bid_volume = std::make_unique<DeleteTriggerProtocol::TotalBidVolumeT>();
            p.cond_total_bid_volume->check = textToBool(tokens[idx++]);
            p.cond_total_bid_volume->volume = std::stoll(tokens[idx++]);

            p.cond_total_ask_volume = std::make_unique<DeleteTriggerProtocol::TotalAskVolumeT>();
            p.cond_total_ask_volume->check = textToBool(tokens[idx++]);
            p.cond_total_ask_volume->volume = std::stoll(tokens[idx++]);

            p.cond_last_price_greater_equal = std::make_unique<DeleteTriggerProtocol::LastPriceGreaterEqualT>();
            p.cond_last_price_greater_equal->check = textToBool(tokens[idx++]);
            p.cond_last_price_greater_equal->price = std::stoll(tokens[idx++]);

            p.cond_last_price_less_equal = std::make_unique<DeleteTriggerProtocol::LastPriceLessEqualT>();
            p.cond_last_price_less_equal->check = textToBool(tokens[idx++]);
            p.cond_last_price_less_equal->price = std::stoll(tokens[idx++]);

            p.cond_trigger_at_time = std::make_unique<DeleteTriggerProtocol::TriggerAtTimeT>();
            p.cond_trigger_at_time->check = textToBool(tokens[idx++]);
            p.cond_trigger_at_time->time_sec = std::stoi(tokens[idx++]);

            std::string key = p.seqno;
            paramMap.put(key, {0, p});
        } catch (const std::exception& e) {
            errorLog(std::string("Error parsing CSV line (DeleteTrigger): ") + line + " Error: " + e.what());
        }
    }
}