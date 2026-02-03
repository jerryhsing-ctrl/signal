#include "strongSingle.h"
#include "indexCalc.h"
#include "type.h"
#include "QuoteSv.h"

StrongSingle::StrongSingle (){
    // topTracker = TopKVolumeTracker(200);


}

bool StrongSingle::on_tick(IndexData &idx, format6Type *f6) {
    topTracker.on_tick(f6->symbol, f6->match.Qty);
    // if (topTracker.inPool[f6->symbol]){
    if (!topTracker.inPool(f6->symbol))
        return false;

    // cout << f6->matchTimeStr << '\n';

    vol_cumu[f6->symbol] += f6->match.Qty;
    
    bool cond1 = eval_price_cond(idx, f6);
    bool cond2 = eval_vol_cond(idx, f6);
    bool cond3 = eval_vwap_cond(idx, f6);
    bool cond4 = eval_extremeFilter_cond(idx, f6);

    return cond1 && cond2 && cond3 && cond4;
}

bool StrongSingle::eval_price_cond(IndexData &idx, format6Type *f6) {

    double priceAmp = (double) (idx.day_high - idx.day_low) / idx.day_low;
    maxPriceAmp = max(priceAmp, maxPriceAmp);

    bool cond1 = maxPriceAmp > 0.04;
    double prev_close = quoteSv->f1mgr.format1Map[f6->symbol].previous_close * 10000;
    bool cond2 = (double) (idx.day_high - prev_close) / prev_close;

    // cout << "   maxPriceAmp " << maxPriceAmp << " prev_close " << prev_close << '\n'; 
    return cond1 || cond2;
}

bool StrongSingle::eval_vol_cond(IndexData &idx, format6Type *f6) {
    // cumuVol += f6->match.Qty;
    
    // if (cumuVol)
    
    // cout << "symbol " <<  f6->symbol << " timeStr " << f6->matchTimeStr << " time_us " << f6->matchTime_us << '\n';
    // cout << "  vol " << cumuVol << " qty " << f6->match.Qty << '\n';
    // cout << "f6->symbol " << f6->symbol << " time " << f6->matchTimeStr << " " << cumuVol << '\n';
    long long total_vol = 0;
    for (int i = 1; i <= DAY_PER_MONTH; i++) {
        long long vol = quoteSv->vol_cum[i].query(f6->symbol, f6->matchTime_us);
        total_vol += vol;
        // cout << vol << '\n';
    }
    long long avg = total_vol / DAY_PER_MONTH;


    bool cond1 = ((double) vol_cumu[f6->symbol] / avg) >= 1.2;

    long long cum_vol_yesterday = quoteSv->vol_cum[1].query(f6->symbol, f6->matchTime_us);
    bool cond2 = ((double) vol_cumu[f6->symbol] / cum_vol_yesterday) >= 2.0;

    bool cond3 = quoteSv->trading_val[1][f6->symbol] > 2000000000;
    // cout << "hi\n";
    // cout << '\n';
    
    // fflush(stdout);
    return cond1 || cond2 || cond3;
}

bool StrongSingle::eval_vwap_cond(IndexData &idx, format6Type *f6) {
    if (f6->matchTimeStr >= 92000000000){
        if (f6->match.Price <= idx.vwap  * 0.995) {
            forbidden = true;
        }
    }
    if (!forbidden)
        return true;
    else 
        return false;
    return true;
}

bool StrongSingle::eval_extremeFilter_cond(IndexData &idx, format6Type *f6) {
    double prev_close = quoteSv->f1mgr.format1Map[f6->symbol].previous_close * 10'000;
    double percentageChg = ((double)f6->match.Price - prev_close) / prev_close;
    // cout << "   percentageChg " << percentageChg << '\n';
    return percentageChg < 0.085;

}