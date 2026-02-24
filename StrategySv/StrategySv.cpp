#include <bits/stdc++.h>
#include "StrategySv.h"
#include "core.h"
#include "IniReader.h"
#include "type.h"


// #define CFG_FILE "../exec/cfg/HWStratSv.cfg"


using namespace std;

StrategySv::StrategySv(QuoteSv *quoteSv) {
    this->quoteSv = quoteSv;

    // strongSingle.f1mgr = &quoteSv->f1mgr;
    strongSingle.quoteSv = quoteSv;
    strongGroup.quoteSv = quoteSv;
    
    
    
    IniReader reader(CFG_FILE);
    std::string tseEnable = reader.Read("TSE", "ENABLE");

    IniReader parameterReader;
    parameterReader.ReadIni("./cfg/parameter.cfg");
    string signalAEnable = parameterReader.Read("SignalA", "enabled");
    signalA_enabled = (signalAEnable == "true");
    cout << "signalA_enabled: [" << signalAEnable << "]" << endl;

    string signalBEnable = parameterReader.Read("SignalB", "enabled");
    signalB_enabled = (signalBEnable == "true");
    cout << "signalB_enabled: [" << signalBEnable << "]" << endl;
    
    string strongSingleEnable = parameterReader.Read("StrongSignal", "enabled");
    strongSingle_enabled = (strongSingleEnable == "true");
    cout << "strongSingle_enabled: [" << strongSingleEnable << "]" << endl;
    

    string strongGroupEnable = parameterReader.Read("StrongGroup", "enabled");
    strongGroup_enabled = (strongGroupEnable == "true");
    cout << "strongGroup_enabled: [" << strongGroupEnable << "]" << endl;

    cout << "tseEnable: [" << tseEnable << "]" << endl;
    market_queue_TSE = new queueType();
    quoteSv->subscribe("", market_queue_TSE, 0);
    if (tseEnable == "true") {
        quoteSv->quoteThread_TSE = thread([this]() {
            set_thread_name("quote TSE");
            this->quoteSv->run("TSE");
        });
    }
    th_TSE = thread([this]() {
        pin_thread_to_core(20);
        set_fifo_priority(99);
        set_thread_name("strat TSE");
        this->run(market_queue_TSE, "TSE");
    });
    
    std::string otcEnable = reader.Read("OTC", "ENABLE");
    cout << "otcEnable: [" << otcEnable << "]" << endl;
    market_queue_OTC = new queueType();
    quoteSv->subscribe("", market_queue_OTC, 1);
    if (otcEnable == "true") {
        quoteSv->quoteThread_OTC = thread([this]() {
            set_thread_name("quote OTC");
            this->quoteSv->run("OTC");
        });
        
    }
    // th_OTC = thread([this]() {
    //     pin_thread_to_core(21);
    //     set_thread_name("strat OTC");
    //     this->run(market_queue_OTC, "OTC");
    // });
    
    std::string hwqEnable = reader.Read("HWQ", "ENABLE");
    cout << "hwqEnable: [" << hwqEnable << "]" << endl;
    market_queue_HWQ = new queueType();
    quoteSv->subscribe("", market_queue_HWQ, 2);
    if (hwqEnable == "true") {
        quoteSv->quoteThread_HWQ = thread([this]() {
            // 
            set_thread_name("quote HWQ");
            // 
            this->quoteSv->run("HWQ");
        });
        
    }
    // th_HWQ = thread([this]() {
    //     pin_thread_to_core(22);
    //     set_thread_name("strat HWQ");
    //     this->run(market_queue_HWQ, "HWQ");
    // });
    
    
}


void StrategySv::run(queueType *market_queue_, string market_name) {
    cout << '\n';
    format6Type *f6;
    bool flag = true;
    static std::atomic<bool> printed(false); // 改為 static atomic
    static std::atomic<int> processed_cnt{0};
    while(1) {
        if ((f6 = market_queue_->front()) != nullptr) {
            
            if (flag) {
                cout << "StrategySv::run started " << market_name << endl;
                flag = false;
            }
            if (f6->tradeCode != 1 || (f6->symbol[0] == '0' && f6->symbol[1] == '0')) { 
                market_queue_->pop();
                processed_cnt++;
                continue;
            }
            
            string &symbol = f6->symbol;
            indexCalc &index_calc = index_calc_map_[symbol];
            IndexData idx = index_calc.calc(f6);

            
            // cout <<  "symbol " << f6->symbol << '\n';

            order.on_tick(f6);
            if (order.stocks[f6->symbol] > 0) {
                processed_cnt++;
                market_queue_->pop();
                continue;
            }

            bool single = strongSingle.on_tick(idx, f6);
            if (!strongSingle_enabled)
                single = false;

            bool group = strongGroup.on_tick(idx, f6);
            if (!strongGroup_enabled)
                 group = false;

            
            MatchType matchType = MatchType::None;
            if (single && group)
                matchType = MatchType::Both;
            else if (single)
                matchType = MatchType::StrongSingle;
            else if (group)
                matchType = MatchType::StrongGroup;
            
       

            MatchType triggerMatchTypeA = MatchType::None;
            bool isSignalA = signalA_map_[symbol].eval(idx, f6, matchType, triggerMatchTypeA, strongSingle, strongGroup, quoteSv);
            if (!signalA_enabled)
                isSignalA = false;


            MatchType triggerMatchTypeB = MatchType::None;
            bool isSignalB = signalB_map_[symbol].eval(idx, f6, matchType, triggerMatchTypeB, strongSingle, strongGroup, quoteSv, order.isStoppedLoss(symbol));
            if (!signalB_enabled)
                isSignalB = false;

            // cout << "   SignalA: " << isSignalA << " SignalB: " << isSignalB << '\n';
 
            if (isSignalA) {
                 order.trigger(idx, f6, entry_idx++, SIGNAL_TYPE::SIGNAL_A, triggerMatchTypeA, strongSingle, strongGroup);
            } else if (isSignalB) {
                 order.trigger(idx, f6, entry_idx++, SIGNAL_TYPE::SIGNAL_B, triggerMatchTypeB, strongSingle, strongGroup);
            }
            else if (isSignalA && isSignalB)
                errorLog(" both signal triggered, symbol: " + f6->symbol + " matchTime: " + to_string(f6->matchTimeStr));
            
            market_queue_->pop(); 
            processed_cnt++;


            bool expected = false;
        // 使用 compare_exchange_strong 確保只有一個 thread 能將 printed 從 false 變成 true
            if (quoteSv->readFileFinish && processed_cnt == quoteSv->readFileCnt && printed.compare_exchange_strong(expected, true)) {
                // sleep(3);
                cout << " total cnt " << processed_cnt << '\n';
                cout << "signal finish\n";
                
                format6Type f6;
                f6.matchTimeStr = LLONG_MAX; 
                
                for (auto& [symbol, qty] : order.stocks) {
                    if (qty > 0) {
                        errorLog(" final stock: " + symbol + " qty: " + to_string(qty));
                        f6.symbol = symbol;
                        order.on_tick(&f6); // 強制檢查是否需要停損或其他離場條件
                    }
                }

                for (auto& [symbol, qty] : order.stocks) {
                    if (qty > 0) {
                        errorLog(" !!!!!after market close : final stock: " + symbol + " qty: " + to_string(qty));
                    }
                }
                cout << " cash left: " <<  order.cash  << '\n';
                fflush(stdout);
                while(1) {
                    cout << "finished all processing, sleeping... " << endl;
                    // sleep(1);
                    exit(0);
                }
            }
        } 
    }
}
