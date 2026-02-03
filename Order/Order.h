#pragma once
#include "type.h"
#include "Format6.h"
#include "indexCalc.h"

#include <fstream>
#include <chrono>

enum SIGNAL_TYPE {
	SIGNAL_A,
	SIGNAL_B
};

class Order {
private:
    std::ofstream logFileA;
    std::ofstream logFileB;
public:
    Order();
    void trigger(IndexData &idx, format6Type *f6, int entry_idx, SIGNAL_TYPE signal_type);
    // void leave(IndexData &idx, format6Type *f6, int entry_idx);
};