#pragma once
#include <unordered_map>
#include <string>
#include "QuoteSv.h"
#include "signalA.h"
#include "signalB.h"
#include "type.h"
#include "indexCalc.h"
#include "Order.h"
#include "strongSingle.h"
#include "strongGroup.h"
#include <unordered_map>


class StrategySv
{
private:
	thread th_TSE, th_OTC, th_HWQ;
	
	// signalA signal_A;
	// signalB signal_B;
	queueType *market_queue_TSE = nullptr;
	queueType *market_queue_OTC = nullptr;
	queueType *market_queue_HWQ = nullptr;
	unordered_map<std::string, indexCalc> index_calc_map_;

	

	unordered_map<std::string, signalA> signalA_map_;
	unordered_map<std::string, signalB> signalB_map_;
	
	int entry_idx = 0;
public:
	StrategySv(QuoteSv *quoteSv);
	void run(queueType *market_queue_, std::string market_name);
	Order order;
	QuoteSv *quoteSv;
	StrongSingle strongSingle;
	StrongGroup strongGroup;
};