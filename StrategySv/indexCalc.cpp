#include "indexCalc.h"
#include "IniReader.h"
#define params "./cfg/parameter.cfg"

using namespace std;

long long min_to_us(double min) {
    return min * 60 * 1000000 + 0.01;
}

indexCalc::indexCalc() {
    IniReader reader(params);
    std::string rolling_low_duration_str = reader.Read("Index", "ROLLING_LOW_DURATION");
    rolling_low_duration = min_to_us(std::stoll(rolling_low_duration_str));
    rolling_low.setDuration(rolling_low_duration);

    std::string rolling_sum_short_duration_str = reader.Read("Index", "ROLLING_SUM_SHORT_DURATION");
    rolling_sum_short_duration = min_to_us(std::stoll(rolling_sum_short_duration_str));
    rolling_sum_short.setDuration(rolling_sum_short_duration);

    std::string rolling_sum_long_duration_str = reader.Read("Index", "ROLLING_SUM_LONG_DURATION");
    rolling_sum_long_duration = min_to_us(std::stoll(rolling_sum_long_duration_str));
    rolling_sum_long.setDuration(rolling_sum_long_duration);
}

IndexData indexCalc::calc(format6Type *f6) {
    if (symbol.empty())
        symbol = f6->symbol;

    IndexData idx;
    
    price_vol_sum += (long long) f6->match.Price * f6->match.Qty;
    vol_sum += f6->match.Qty;
    idx.vwap = (double) price_vol_sum / vol_sum;

    long long inner_vol = (f6->tradeAt == 1) ? f6->match.Qty : 0;
    rolling_sum_short.update(f6->matchTime_us, inner_vol);
    long long rolling_sum_short_ll = rolling_sum_short.getSum();

    rolling_sum_long.update(f6->matchTime_us, inner_vol);
    long long rolling_sum_long_ll = rolling_sum_long.getSum();


    idx.rolling_sum_ratio = ((double) rolling_sum_short_ll / ((double) rolling_sum_long_ll)) * (rolling_sum_long_duration / rolling_sum_short_duration);
    // cout << " ration " << idx.rolling_sum_ratio << " " << rolling_sum_short_ll << " " << rolling_sum_long_ll << " " << rolling_sum_long_duration << " " << rolling_sum_short_duration << '\n';
    rolling_low.update(f6->matchTime_us, f6->match.Price);
    idx.rolling_low = rolling_low.getLow();

    day_high = max(day_high, (long long)f6->match.Price);
    idx.day_high = day_high;

    day_low = min(day_low, (long long) f6->match.Price);
    idx.day_low = day_low;


    return idx;
}