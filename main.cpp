#include <unistd.h>
#include <iostream>
#include <string>
#include "StrategySv.h"
#include "core.h"

using namespace std;
int main() {
    pin_thread_to_core(1);
    set_fifo_priority(99);
    const string tradeDate = "20260224";
    QuoteSv quoteSv = QuoteSv();
    quoteSv.f1mgr.readFile(tradeDate);
    quoteSv.checkPrevDayLimitUp(tradeDate);
    
    // for (auto &[symbol, prevDayLimitUp] : quoteSv.prevDayLimitUpMap) {
    //     if (prevDayLimitUp)
    //         cout << " symbol: " << symbol << " prevDayLimitUp: " << prevDayLimitUp << endl;
    // }

    StrategySv strategySv = StrategySv(&quoteSv);
    strategySv.order.setDate(tradeDate);
    cout << "start To readFile\n";
    
    strategySv.quoteSv->getTickData("OTC", tradeDate);
    strategySv.quoteSv->getTickData("TSE", tradeDate);
    strategySv.strongGroup.getGroup();

    strategySv.quoteSv->readFileMerged("OTC", tradeDate, "TSE", tradeDate);

    fflush(stdout);
    while(1) {
        sleep(10);
    }
}
