#pragma once
#include "topTracker.h"
#include <unordered_map>
#include "type.h"
#include "indexCalc.h"
#include "Format1.h"
#include "QuoteSv.h"

class StrongSingle {
public:
    StrongSingle();
    TopKVolumeTracker topTracker;

    bool on_tick(IndexData &idx, format6Type *f6);
    unordered_map<std::string, bool> is_strong_stock;



    Format1Manager *f1mgr;
    QuoteSv *quoteSv;
private:
    double maxPriceAmp = 0;
    bool eval_price_cond(IndexData &idx, format6Type *f6);
    // long long cumuVol = 0;
    unordered_map<std::string, long long> vol_cumu;
    bool eval_vol_cond(IndexData &idx, format6Type *f6);
    bool forbidden = false;
    bool eval_vwap_cond(IndexData &idx, format6Type *f6);
    bool eval_extremeFilter_cond(IndexData &idx, format6Type *f6);
};