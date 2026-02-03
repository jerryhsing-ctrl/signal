#include <unistd.h>
#include <iostream>
#include <string>
#include "StrategySv.h"
#include "core.h"

using namespace std;
int main() {
    pin_thread_to_core(14);
    set_fifo_priority(99);
    StrategySv strategySv = StrategySv();
    // errorLog("main");
    // sleep(1);
    cout << "start To readFile\n";
    fflush(stdout);
    strategySv.quoteSv->readFile("./data/TSEQuote.20260129");
    // strategySv.quoteSv->readFile("./data/OTCQuote.20260128");
    while(1) {
        sleep(10);
    }

}
