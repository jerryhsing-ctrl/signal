#pragma once
#include <string>
#include "Format6.h"
#include "type.h"
#include "rolling.h"

struct IndexData {
public:
    double vwap = 0;
    double rolling_sum_ratio = 0;
    long long rolling_low = 0;
    long long day_high = 0;
    long long day_low = LLONG_MAX;
};

class indexCalc {
public:

    indexCalc();
    IndexData calc(format6Type *f6);
    string symbol = "";
    
private:

    RollingLow<long long, long long> rolling_low;
    RollingSum<long long, long long> rolling_sum_short;
    RollingSum<long long, long long> rolling_sum_long;

    long long price_vol_sum = 0;
    long long vol_sum = 0;
    long long day_high = 0;
    long long day_low = LLONG_MAX;

    double rolling_low_duration;
    double rolling_sum_short_duration;
    double rolling_sum_long_duration;
};