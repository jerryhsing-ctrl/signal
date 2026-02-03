
#pragma once



#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include <signal.h>
#include "IniReader.h"
#include "UDPSession.h"
#include "Format6.h"
#include <shared_mutex>
#include "timeUtil.h"
#include "type.h"
#include "pool.h"
#include "Format1.h"
#include "UDPSession.h"
#include <atomic>
#include "volTracker.h"
// #define RANGE 10000

#define DAY_PER_MONTH 20

using namespace std;

using Format6Callback = std::function<void(Format6)>;

class QuoteSv
{
	private:
		const int MaxBuf = 5120;
		bool  isTerminated = false;
		
		int lastId = 0;

		const char *market = nullptr;
		
		UDPSession *tseSession = nullptr, *otcSsession = nullptr, *hwqSsession = nullptr;

		
		// SharedMap<int, queueType*> subsMap;
		// queueType* subsMap[RANGE];
		// unordered_map<std::string, queueType*> subsMap[2];
		queueType* quoteQue[3];

        void CreateSession(UDPSession*&, const char*, int);
		void PackageCallback(const char *, unsigned char* data, size_t len, timeType timeStamp, int idx);				
		
    public:
        QuoteSv();
        ~QuoteSv();

		vector<LinearVolumeTracker> vol_cum;
		vector<LinearVolumeTracker> tradingValue_cum;
		vector<unordered_map<std::string, long long>> trading_val;
		

		bool get_vol_cum(std::string filename, int idx);

		Format1Manager f1mgr;
        atomic<int> readFileCnt = 0;
        atomic<bool> readFileFinish = false;

		void getPastData();

		bool readFile(const std::string filename);
		
		bool run(string market);
		thread quoteThread_TSE, quoteThread_OTC, quoteThread_HWQ;
		// 訂閱
		void subscribe(const std::string& symbol, queueType* queue, int idx);
		// 取消訂閱
		void unsubscribe(const std::string& symbol, int idx);
};
