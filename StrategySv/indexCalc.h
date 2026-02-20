#pragma once
#include <climits>
#include <string>
#include "Format6.h"
#include "type.h"
#include "rolling.h"

struct IndexData {
public:
    double vwap = 0;
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

    long long price_vol_sum = 0;
    long long vol_sum = 0;
    long long day_high = 0;
    long long day_low = LLONG_MAX;
};