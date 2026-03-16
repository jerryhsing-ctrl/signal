#include <unistd.h>
#include <iostream>
#include <string>
#include <chrono>
#include "StrategySv.h"
#include "core.h"

using namespace std;
using hrclock = std::chrono::high_resolution_clock;
static double elapsed_ms(hrclock::time_point t0) {
    return std::chrono::duration<double, std::milli>(hrclock::now() - t0).count();
}

int main(int argc, char* argv[]) {
    auto t_start = hrclock::now();
    pin_thread_to_core(1);
    set_fifo_priority(99);
    string tradeDate = "20260225";
    string logFolder = "";
    if (argc > 1) {
        tradeDate = argv[1];
    }
    if (argc > 2) {
        logFolder = argv[2];
    }

    auto t0 = hrclock::now();
    QuoteSv quoteSv = QuoteSv();
    quoteSv.f1mgr.readFile(tradeDate);
    quoteSv.checkPrevDayLimitUp(tradeDate);
    printf("[TIMING] init+f1mgr+prevDayLU: %.0f ms\n", elapsed_ms(t0));

    // for (auto &[symbol, prevDayLimitUp] : quoteSv.prevDayLimitUpMap) {
    //     if (prevDayLimitUp)
    //         cout << " symbol: " << symbol << " prevDayLimitUp: " << prevDayLimitUp << endl;
    // }

    StrategySv strategySv = StrategySv(&quoteSv);
    strategySv.order.setDate(tradeDate, logFolder);
    cout << "start To readFile\n";

    t0 = hrclock::now();
    strategySv.quoteSv->getTickData("OTC", tradeDate);
    printf("[TIMING] getTickData OTC: %.0f ms\n", elapsed_ms(t0));

    t0 = hrclock::now();
    strategySv.quoteSv->getTickData("TSE", tradeDate);
    printf("[TIMING] getTickData TSE: %.0f ms\n", elapsed_ms(t0));

    t0 = hrclock::now();
    strategySv.strongGroup.getGroup();
    printf("[TIMING] getGroup: %.0f ms\n", elapsed_ms(t0));

    for (auto& [sym, valid] : strategySv.strongGroup.symbol_is_valid) {
        if (valid) quoteSv.tickFilter.insert(sym);
    }
    quoteSv.tickFilter.insert("0050");
    printf("tickFilter: %zu symbols (from %zu group members)\n",
           quoteSv.tickFilter.size(),
           strategySv.strongGroup.symbol_is_valid.size());

    t0 = hrclock::now();
    strategySv.quoteSv->readFileMerged("OTC", tradeDate, "TSE", tradeDate);
    printf("[TIMING] readFileMerged: %.0f ms\n", elapsed_ms(t0));
    printf("[TIMING] TOTAL: %.0f ms\n", elapsed_ms(t_start));

    fflush(stdout);

    // Wait for strategy thread to finish processing all ticks
    // StrategySv::run() calls exit(0) after generating report
    // Fallback: if strategy thread gets stuck, exit after 60s
    for (int i = 0; i < 60; i++) {
        sleep(1);
    }
    fprintf(stderr, "[main] fallback exit after 60s\n");
    exit(0);
}
