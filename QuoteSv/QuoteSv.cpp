#include "QuoteSv.h"
#include "Format1.h"
#include "type.h"
#include "timeUtil.h"
#include "errorLog.h"
#include <pthread.h>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <string>
#include <filesystem>
#include <fstream>
#include "core.h"
// #define CFG_FILE "../exec/cfg/HWStratSv.cfg"

using namespace std;

// QuoteSv::QuoteSv()
// {
// 	f1mgr.readFileToday();

// 	vol_cum.resize(DAY_PER_MONTH + 1);
// 	trading_val.resize(DAY_PER_MONTH + 1);
// 	get_vol_cum("./data/TSEQuote.20260129", 0);

// 	get_vol_cum("./data/TSEQuote.20260128", 1);
// 	get_vol_cum("./data/TSEQuote.20260127", 2);
// 	get_vol_cum("./data/TSEQuote.20260126", 3);

// 	get_vol_cum("./data/TSEQuote.20260123", 4);
// 	get_vol_cum("./data/TSEQuote.20260122", 5);
// 	get_vol_cum("./data/TSEQuote.20260121", 6);
// 	get_vol_cum("./data/TSEQuote.20260120", 7);
// 	get_vol_cum("./data/TSEQuote.20260119", 8);

// 	get_vol_cum("./data/TSEQuote.20260116", 9);
// 	get_vol_cum("./data/TSEQuote.20260115", 10);
// 	get_vol_cum("./data/TSEQuote.20260114", 11);
// 	get_vol_cum("./data/TSEQuote.20260113", 12);
// 	get_vol_cum("./data/TSEQuote.20260112", 13);

// 	get_vol_cum("./data/TSEQuote.20260109", 14);
// 	get_vol_cum("./data/TSEQuote.20260108", 15);
// 	get_vol_cum("./data/TSEQuote.20260107", 16);
// 	get_vol_cum("./data/TSEQuote.20260106", 17);
// 	get_vol_cum("./data/TSEQuote.20260105", 18);

// 	get_vol_cum("./data/TSEQuote.20260102", 19);
// 	get_vol_cum("./data/TSEQuote.20251231", 20);

	
	
	

// }



QuoteSv::QuoteSv()
{
	
	quoteQue[0] = quoteQue[1] = quoteQue[2] = nullptr;
	vol_cum.resize(DAY_PER_MONTH + 1);
    tradingValue_cum.resize(DAY_PER_MONTH + 1);
	trading_val.resize(DAY_PER_MONTH + 1);
    f1mgr.readFileToday();
	
    
}

void QuoteSv::getPastData() {

	// 1. 關鍵：預先分配記憶體，避免 Race Condition
    

    // 2. 定義工作結構與清單
    struct Task {
        string filename;
        int idx;
    };

    // 將原本散落的讀取任務整理成清單
    vector<Task> tasks = {
        {"./data/TSEQuote.20260129", 0},
        {"./data/TSEQuote.20260128", 1},
        {"./data/TSEQuote.20260127", 2},
        {"./data/TSEQuote.20260126", 3},
        {"./data/TSEQuote.20260123", 4},
        {"./data/TSEQuote.20260122", 5},
        {"./data/TSEQuote.20260121", 6},
        {"./data/TSEQuote.20260120", 7},
        {"./data/TSEQuote.20260119", 8},
        {"./data/TSEQuote.20260116", 9},
        {"./data/TSEQuote.20260115", 10},
        {"./data/TSEQuote.20260114", 11},
        {"./data/TSEQuote.20260113", 12},
        {"./data/TSEQuote.20260112", 13},
        {"./data/TSEQuote.20260109", 14},
        {"./data/TSEQuote.20260108", 15},
        {"./data/TSEQuote.20260107", 16},
        {"./data/TSEQuote.20260106", 17},
        {"./data/TSEQuote.20260105", 18},
        {"./data/TSEQuote.20260102", 19},
        {"./data/TSEQuote.20251231", 20}
    };

    // 3. 設定執行緒數量與原子計數器
    const int num_threads = 5; 
    std::atomic<size_t> task_counter{0}; // 指向下一個要處理的任務索引
    std::vector<std::thread> workers;
    workers.reserve(num_threads);

    // 4. 定義 Worker 行為 (Lambda)
    auto worker_func = [&](int thread_id, int core) {
        // 迴圈：只要還有任務就繼續拿

        while (true) {
            // fetch_add 會原子性地取出當前值並+1，保證不重複
			pin_thread_to_core(core);
            size_t current_idx = task_counter.fetch_add(1);

            // 如果索引超過任務總數，代表做完了，離開迴圈
            if (current_idx >= tasks.size()) {
                break;
            }

            // 執行讀檔任務
            printf("Thread %d processing %s\n", thread_id, tasks[current_idx].filename.c_str());
            this->get_vol_cum(tasks[current_idx].filename, tasks[current_idx].idx);
        }
    };

    // 5. 啟動 5 個執行緒
    // for (int i = 0; i < num_threads; ++i) {
        
    // }
	workers.emplace_back(worker_func, 0, 15);
	workers.emplace_back(worker_func, 1, 16);
	workers.emplace_back(worker_func, 2, 17);
	workers.emplace_back(worker_func, 3, 18);
	workers.emplace_back(worker_func, 4, 19);

    // 6. 等待所有執行緒完成
    for (auto& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }

    printf("QuoteSv init finished. Total files processed: %lu\n", tasks.size());
}

bool QuoteSv::get_vol_cum(string filename, int idx) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		errorLog("QuoteSv::readFile, open file fail: " + filename);
		return false;
	}

	std::string lineTrade, lineDepth;
	lineTrade.reserve(1024); // 預先配置記憶體以減少重新分配
    lineDepth.reserve(1024);

	format6Type f6;
	// int cu = 0;
	while (std::getline(file, lineTrade)) {
		if (lineTrade.empty()) continue;
		if (lineTrade.size() < 2 || lineTrade[0] != 'T' || lineTrade[1] != 'r') continue;
        
		
		f6.fromRedis(lineTrade, "");
		if (f6.statusCode == 0) {
			// if (f6.symbol == "1605") {
			// 	cu += f6.match.Qty;
			// }
			vol_cum[idx].on_tick(f6.symbol, f6.matchTime_us, f6.match.Qty);
			tradingValue_cum[idx].on_tick(f6.symbol, f6.matchTime_us, f6.match.Price * f6.match.Qty);
			trading_val[idx][f6.symbol] += f6.match.Qty * f6.match.Price;
			
		}
		
	}

	file.close();
	return true;
}
//---------------------------------------------------------------------------------------------------------------------------
QuoteSv::~QuoteSv()
{

}



//---------------------------------------------------------------------------------------------------------------------------
void QuoteSv::CreateSession(UDPSession* &udpSession, const char* cfgSession, int idx)
{
	//讀ini檔案
	
	IniReader *reader = new IniReader (CFG_FILE);
	printf("CreateSession, LoadCfg cfgFile=[%s].\n", CFG_FILE);         
	
    //共用部份
    std::string myIP = reader->Read(cfgSession,"MYIP");    
    // logger->AddLog("[%s] MYIP = %s", cfgSession, myIP.c_str());
	
	std::string udpHost = reader->Read(cfgSession,"IP");	
	// logger->AddLog("[%s] MKT_IP = %s", cfgSession, udpHost.c_str());
	
	int udpPort = atoi(reader->Read(cfgSession,"PORT").c_str());
	// logger->AddLog("[%s] MKT_PORT = %i", cfgSession, udpPort);   		
	
	// 連到報價線路     
	udpSession = new UDPSession(cfgSession,
		[this](const char *market, unsigned char* data, size_t len, timeType timeStamp, int idx) {
			this->PackageCallback(market, data, len, timeStamp, idx);
		},
		idx
	);	
	//只要格式六
	// std::list<unsigned short> formatList = {6};
	// udpSession->SetFilter(formatList);
	
	if (!udpSession->Open(myIP.c_str(), udpHost.c_str(), udpPort))
		errorLog("open receiver fail.");
		
	delete reader;
}
bool QuoteSv::run(string market) {
	UDPSession *session = nullptr;
	if (market == "TSE") {
		if (tseSession == nullptr) {
			CreateSession(tseSession, "TSE", 0);
		}
		session = tseSession;
	}
	else if (market == "OTC") {
		if (otcSsession == nullptr) {
			CreateSession(otcSsession, "OTC", 1);
		}
		session = otcSsession;
	}
	else if (market == "HWQ") {
		if (hwqSsession == nullptr) {
			CreateSession(hwqSsession, "HWQ", 2);
		}
		session = hwqSsession;
	}
	else {
		errorLog("QuoteSv::run, market not support: " + market);
		return false;
	}

	session->ReceiveLoop();
	return true;
}

//---------------------------------------------------------------------------------------------------------------------------
void QuoteSv::PackageCallback(const char *market, unsigned char* data, size_t len, timeType timeStamp, int idx) 
{

	
	string symbol = "";
	char *c = (char*) (data + 10);
	for (int i = 0; i < 6; i++) {
		if (*c != ' ') {
			symbol += *c;
		}
		c++;  // 不管是不是空白都要前進
	}
	if (symbol == "") {
		
		errorLog("QuoteSv::PackageCallback, symbol empty");
	}
	if (symbol.empty()) {
		cout << "Empty\n";
		string symbol = "";
		char *c = (char*) (data + 10);
		for (int i = 0; i < 6; i++) {
			if (*c == ' ')
				symbol += *c;
			c++;
		}
		cout << symbol << '\n';
	}

	auto queue = quoteQue;
	if (queue == nullptr)
		return;  // 同樣改成 continue

	format6Type *f6 = queue[idx]->alloc();
	// format6Type *f6 = new format6Type();
	f6->init(data, len);
	// f6->
	if (f6->symbol.empty()) {
		errorLog("QuoteSv::PackageCallback, f6 symbol empty after init");
	}
	f6->time = timeStamp;
	if (f6->statusCode == 0) {
		queue[idx]->push();
	}
}

bool QuoteSv::readFile(const std::string filename)
{
	std::ifstream file(filename);
	if (!file.is_open()) {
		errorLog("QuoteSv::readFile, open file fail: " + filename);
		return false;
	}

	std::string lineTrade, lineDepth, tmp;
	lineTrade.reserve(1024); // 預先配置記憶體以減少重新分配
    lineDepth.reserve(1024);

    int idx = 0; // TODO: 確認 idx 來源，暫設為 0

	bool lineStored = false;
	while (true) {
		if (lineStored) {
			lineTrade = tmp;
		}
		else {
			if (!std::getline(file, lineTrade)) 
				break;
		}
		lineStored = false;

		if (lineTrade.empty()) continue;
		// if (lineTrade.substr(0, 5) != "Trade")
		// 	cout << lineTrade << '\n';
		if (lineTrade.size() < 2 || lineTrade[0] != 'T' || lineTrade[1] != 'r') continue;
		if (!getline(file, lineDepth))
			lineDepth = "";

		if (lineDepth.size() >= 2 && lineDepth[0] == 'T' && lineDepth[1] == 'r') {
			tmp = lineDepth;
			lineDepth = "";
			lineStored = true;
		}
		if (lineDepth != "" && lineTrade.substr(5, 20) != lineDepth.substr(5, 20)) {
			lineDepth = "";
		}
        
		if (quoteQue[idx]) {
			format6Type *f6 = quoteQue[idx]->alloc();
			if (f6->fromRedis(lineTrade, lineDepth)) {
				if (f6->statusCode == 0) {
                    readFileCnt++;
					
					quoteQue[idx]->push();
					// if (f6->symbol == "1605") {
					// cout << "trade [" << lineTrade << "]\n";
					// cout << "depth [" << lineDepth << "]\n";
					// cout << '\n'; 
					// }
				}
			}
		}
		else {
			errorLog("quoteQue is NULL");
		}
	}
    readFileFinish = true;
    cout << " readFileFinish cnt = " << readFileCnt << '\n';
	fflush(stdout);
	file.close();
	return true;
}
//---------------------------------------------------------------------------
// 訂閱
void QuoteSv::subscribe(const std::string& symbol, queueType *queue, int idx)
{
	// int symbol = stoi(symbol_);
	// int symbol = stoi(symbol_);
	// if (subsMap[idx][symbol] == nullptr) {
	// 	subsMap[idx][symbol] = queue;
	// }
	if (quoteQue[idx] == nullptr)
		quoteQue[idx] = queue;
}
//---------------------------------------------------------------------------
// 取消訂閱
void QuoteSv::unsubscribe(const std::string& symbol, int idx)
{
	// int symbol = stoi(symbol_);
	// subsMap[idx][symbol] = nullptr;
}