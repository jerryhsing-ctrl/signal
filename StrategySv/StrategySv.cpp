#include <bits/stdc++.h>
#include "StrategySv.h"
#include "core.h"
#include "IniReader.h"
#include "type.h"


// #define CFG_FILE "../exec/cfg/HWStratSv.cfg"


using namespace std;

StrategySv::StrategySv() {
    quoteSv = new QuoteSv();
    quoteSv->getPastData();
    // strongSingle.f1mgr = &quoteSv->f1mgr;
    strongSingle.quoteSv = quoteSv;
    strongGroup.quoteSv = quoteSv;
    strongGroup.getData();
    
    
    IniReader reader(CFG_FILE);
    std::string tseEnable = reader.Read("TSE", "ENABLE");
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
    th_OTC = thread([this]() {
        pin_thread_to_core(21);
        set_thread_name("strat OTC");
        this->run(market_queue_OTC, "OTC");
    });
    
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
    th_HWQ = thread([this]() {
        pin_thread_to_core(22);
        set_thread_name("strat HWQ");
        this->run(market_queue_HWQ, "HWQ");
    });
    
    
}


void StrategySv::run(queueType *market_queue_, string market_name) {
    cout << '\n';
    format6Type *f6;
    bool flag = true;
    int cnt = 0;
    bool printed = false;
    while(1) {
        if ((f6 = market_queue_->front()) != nullptr) {
            cnt++;



            if (flag) {
                cout << "StrategySv::run started " << market_name << endl;
                flag = false;
            }
            if (f6->tradeCode != 1) { 
                market_queue_->pop();
                continue;
            }

            // if (f6->symbol != "1605") {
            //     market_queue_->pop();
            //     continue;
            // }

            string &symbol = f6->symbol;
            indexCalc &index_calc = index_calc_map_[symbol];



            IndexData idx = index_calc.calc(f6);


            bool choosen = false;
            if (strongSingle.on_tick(idx, f6)) {
                choosen = true;
            }
            if (strongGroup.on_tick(idx, f6)) {
                choosen = true;
            }
            choosen = true;

            if (signalA_map_[symbol].eval(idx, f6, choosen)) {
                order.trigger(idx, f6, entry_idx++, SIGNAL_A);
            }
            // if (signalA_map_[symbol].leave(idx, f6)) {
            //     order.leave(idx, f6, entry_idx);
            // }

            // if (signalB_map_[symbol].eval(idx, f6)) {
            //     order.trigger(idx, f6, entry_idx++, SIGNAL_B);
            // }

            // if (signalB_map_[symbol].leave(idx, f6)) {
            //     order.leave(idx, f6, entry_idx);
            // }

            // order.trigger(idx, f6, entry_idx);
            // cout << "  cnt = " << cnt << '\n'; 
            market_queue_->pop(); 
            // if (cnt % 10000 == 0) // 1760420
            //     cout << "cnt " << cnt << '\n';
            
        }
        if (quoteSv->readFileFinish &&  cnt == quoteSv->readFileCnt && !printed) {
            cout << " total cnt " << cnt << '\n';
            cout << "signal finish\n";
            printed = true;
            fflush(stdout);
            exit(0);
        }
    }
}
