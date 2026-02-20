#include "indexCalc.h"
#include "IniReader.h"
#define params "./cfg/parameter.cfg"

using namespace std;

// long long min_to_us(double min) {
//     return min * 60 * 1000000 + 0.01;
// }

indexCalc::indexCalc() {
}

IndexData indexCalc::calc(format6Type *f6) {
    if (symbol.empty())
        symbol = f6->symbol;

    IndexData idx;
    
    price_vol_sum += (long long) f6->match.Price * f6->match.Qty;
    vol_sum += f6->match.Qty;
    idx.vwap = (double) price_vol_sum / vol_sum;



    day_high = max(day_high, (long long)f6->match.Price);
    idx.day_high = day_high;

    day_low = min(day_low, (long long) f6->match.Price);
    idx.day_low = day_low;


    return idx;
}