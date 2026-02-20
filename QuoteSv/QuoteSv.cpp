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
#include <atomic>
#include <thread>
#include <immintrin.h>
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



QuoteSv::QuoteSv() : numTracker(10 * 60 * 1000 * 1000LL) // 10 分鐘的 Window
{
	
	quoteQue[0] = quoteQue[1] = quoteQue[2] = nullptr;
	vol_cum.resize(DAY_PER_MONTH + 1);
    tradingValue_cum.resize(DAY_PER_MONTH + 1);
	trading_val.resize(DAY_PER_MONTH + 1);
    
	// numTracker = NumTracker(10 * 60 * 1000 * 1000LL); // 10 分鐘的 Window
    
}

void QuoteSv::getTickData(string type, string date) {

	// 1. 關鍵：預先分配記憶體，避免 Race Condition
    

    // 2. 定義工作結構與清單
    struct Task {
        string filename;
        int idx;
    };

    // 0. 只掃描一次目錄，建立日期 -> 檔案路徑的對應表
    // Key: 日期字串 (YYYYMMDD), Value: 完整檔案路徑
    std::map<std::string, std::string> dateFileMap;
    std::string dataDir = "./data/";
    string prefix = type + "Quote";

    if (!std::filesystem::exists(dataDir)) {
        errorLog("Data directory not found: " + dataDir);
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dataDir)) {
        std::string filename = entry.path().filename().string();
        if (filename.rfind(prefix, 0) == 0) { // Check if starts with prefix
             // 檔名格式 [Type]Quote.YYYYMMDD
             // 取得 YYYYMMDD 部分
             std::string fileDate = filename.substr(prefix.length() + 1);
             if (fileDate.length() == 8) {
                 dateFileMap[fileDate] = entry.path().string();
             }
        }
    }

    if (dateFileMap.empty()) {
        errorLog("No " + prefix +  " files found in " + dataDir);
        return;
    }

    // 1. 找出包含目標日期與過去日期的所有檔案
    // 目標：找到目標日期 (含) 以前的 21 個交易日（idx 0~20）
    // 因為 map 是按 key (日期) 排序的，我們可以用 upper_bound 找到目標日期的下一個位置，然後往前倒數
    
    // 指到 "大於" date 的第一個元素
    auto it = dateFileMap.upper_bound(date); 
    
    // 如果 it == begin，表示目錄中所有檔案的日期都比 date 大（沒有符合的歷史資料）
    if (it == dateFileMap.begin()) {
        errorLog("No historical data found for date " + date);
        return;
    }

    // 往前找，收集最多 21 天的任務
    vector<Task> tasks;
    int neededDays = 21; // 0 (今日) + 20 (過去)

    // 從 upper_bound 的前一個開始 (因為 upper_bound 是大於目標的第一個)
    // 這樣如果是目標當天有檔案，就會是前一個，如果當天沒檔案，前一個就是最近的過去交易日
    for (int i = 0; i < neededDays; ++i) {
        --it; // 往前推一天
        
        // 檢查是否還在範圍內
        // 如果上面 --it 之前 it 已經是 begin，--it 行為是 undefined (但這邊我們有檢查過 it != begin)
        // 修正邏輯：我們需要安全的迭代器倒退
        
        tasks.push_back({it->second, i});

        // 檢查是否到達 map 開頭
        if (it == dateFileMap.begin()) break;
    }

    // 將原本散落的讀取任務整理成清單
    /*
    vector<Task> tasks = {
        {"./data/OTCQuote.20260204", 0},
        ...
    };
    */

   cout << "getTickData: processing " << tasks.size() << " files for date " << date << endl;
    for (const auto& t : tasks) {
        cout << "  idx " << t.idx << ": " << t.filename << endl;
    }

    // 3. 設定執行緒數量與原子計數器
    const int num_threads = 5; 
    std::atomic<size_t> task_counter{0}; // 指向下一個要處理的任務索引
    std::vector<std::thread> workers;
    workers.reserve(num_threads);

    // 4. 定義 Worker 行為 (Lambda)
    auto worker_func = [&](int thread_id, int core) {
        // 迴圈：只要還有任務就繼續拿
		set_thread_name(("QuoteSvWorker" + std::to_string(thread_id)).c_str());
		set_fifo_priority(99);
        while (true) {
            // fetch_add 會原子性地取出當前值並+1，保證不重複
			pin_thread_to_core(core);
            size_t current_idx = task_counter.fetch_add(1);

            // 如果索引超過任務總數，代表做完了，離開迴圈
            if (current_idx >= tasks.size()) {
                break;
            }

            // 執行讀檔任務
            // printf("Thread %d processing %s\n", thread_id, tasks[current_idx].filename.c_str());
            this->get_vol_cum(tasks[current_idx].filename, tasks[current_idx].idx);
        }
    };

    // 5. 啟動 5 個執行緒
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back(worker_func, i + 3, i + 3);
    }
	// workers.emplace_back(worker_func, 0, 15);
	// workers.emplace_back(worker_func, 1, 16);
	// workers.emplace_back(worker_func, 2, 17);
	// workers.emplace_back(worker_func, 3, 18);
	// workers.emplace_back(worker_func, 4, 19);

    // 6. 等待所有執行緒完成
    for (auto& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }

    printf("QuoteSv init finished. Total files processed: %lu\n", tasks.size());
}

bool QuoteSv::get_vol_cum(string filename, int idx) {
	std::ifstream file;
	const size_t BUF_SIZE = 1024 * 1024; // 1MB Buffer
	std::vector<char> buffer(BUF_SIZE);
	file.rdbuf()->pubsetbuf(buffer.data(), BUF_SIZE);

	file.open(filename);
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
			tradingValue_cum[idx].on_tick(f6.symbol, f6.matchTime_us, (long long) f6.match.Price * f6.match.Qty / 10);
			trading_val[idx][f6.symbol] += (long long) f6.match.Qty * f6.match.Price / 10;
			
		}
		
	}


	file.close();
	return true;
}
bool QuoteSv::checkPrevDayLimitUp(const std::string& date) {
	prevDayLimitUpMap.clear();

	// 1. 掃描 ./files/ 目錄，找出 date 之前最近的一個日期檔案
	std::string dir = "./files/";
	std::string prefix = "Symbols_";
	std::string suffix = ".csv";
	std::string prevDate;

	if (!std::filesystem::exists(dir)) {
		// std::cerr << "checkPrevDayLimitUp: directory not found: " << dir << std::endl;
        errorLog("checkPrevDayLimitUp: directory not found: " + dir);
		return false;
	}

	for (const auto& entry : std::filesystem::directory_iterator(dir)) {
		std::string filename = entry.path().filename().string();
		// 檢查檔名格式: Symbols_YYYYMMDD.csv
		if (filename.find(prefix) == 0 &&
			filename.size() > prefix.size() + suffix.size() &&
			filename.rfind(suffix) == filename.size() - suffix.size()) {
			std::string fileDate = filename.substr(prefix.size(),
				filename.size() - prefix.size() - suffix.size());
			if (fileDate.size() == 8 && fileDate < date) {
				if (prevDate.empty() || fileDate > prevDate) {
					prevDate = fileDate;
				}
			}
		}
	}

	if (prevDate.empty()) {
		// std::cerr << "checkPrevDayLimitUp: No previous day file found for date " << date << std::endl;
        errorLog("checkPrevDayLimitUp: No previous day file found for date " + date);
		return false;
	}

	std::cout << "checkPrevDayLimitUp: date=" << date << ", prevDate=" << prevDate << std::endl;

	// 2. 讀取當天檔案 (取 previous_close)
	Format1Manager todayMgr;
	if (!todayMgr.readFile(date)) {
		// std::cerr << "checkPrevDayLimitUp: Cannot read file for date " << date << std::endl;
        errorLog("checkPrevDayLimitUp: Cannot read file for date " + date);
		return false;
	}

	// 3. 讀取前一天檔案 (取 limit_up_price)
	Format1Manager prevMgr;
	if (!prevMgr.readFile(prevDate)) {
		// std::cerr << "checkPrevDayLimitUp: Cannot read file for date " << prevDate << std::endl;
        errorLog("checkPrevDayLimitUp: Cannot read file for date " + prevDate);
		return false;
	}

	// 4. 比較：today.previous_close == prev.limit_up_price 即為前一天漲停
	for (const auto& [symbol, todayData] : todayMgr.format1Map) {
        // cout << "  Checking symbol: " << symbol << endl;
		auto it = prevMgr.format1Map.find(symbol);
		if (it != prevMgr.format1Map.end()) {
			prevDayLimitUpMap[symbol] = (todayData.previous_close == it->second.limit_up_price);
		}
        // if (prevDayLimitUpMap.find(symbol) != prevDayLimitUpMap.end()) {
        //     cout << "    previous_close: " << todayData.previous_close
        //          << ", limit_up_price: " << it->second.limit_up_price
        //          << ", isLimitUp: " << (prevDayLimitUpMap[symbol] ? "Yes" : "No") << endl;
        // }
		// 前一天不存在這檔股票 → 不加入 map
	}

	// return prevDayLimitUpMap;
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

bool QuoteSv::readFile(string market, const std::string date)
{
	string filename = "./data/" + market + "Quote." + date;

	std::ifstream file;
	const size_t BUF_SIZE = 1024 * 1024; // 1MB Buffer
	std::vector<char> buffer(BUF_SIZE);
	file.rdbuf()->pubsetbuf(buffer.data(), BUF_SIZE);

	file.open(filename);
	if (!file.is_open()) {
		errorLog("QuoteSv::readFile, open file fail: " + filename);
		return false;
	}
	// std::ifstream file(filename);
	// if (!file.is_open()) {
	// 	errorLog("QuoteSv::readFile, open file fail: " + filename);
	// 	return false;
	// }

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
			format6Type tmp;
			tmp.market = market;
			if (tmp.fromRedis(lineTrade, lineDepth)) {
				
				if (tmp.statusCode == 0) {
                    format6Type *f6 = quoteQue[idx]->alloc();
					*f6 = tmp;	
					quoteQue[idx]->push();
					lastPrice[f6->symbol] = f6->match.Price;
					readFileCnt++;
					f6->prevLimitUp = prevDayLimitUpMap[f6->symbol];
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

#include <thread>
#include <atomic>
#include <immintrin.h> // For _mm_pause()

// 定義狀態常數
enum ThreadState {
    STATE_NEED_DATA = 0, // 主執行緒已拿走資料，Worker 需要讀下一筆
    STATE_HAS_DATA = 1,  // Worker 已讀完並解析，等待主執行緒合併
    STATE_FINISHED = 2   // 檔案讀取結束 (EOF)
};

void QuoteSv::readFileMerged(string marketA, string dateA, string marketB, string dateB) {
    
    // 1. 準備原子變數與暫存區
    // alignas(64) 避免 False Sharing (Cache line 競爭)
    struct alignas(64) ThreadContext {
        std::atomic<int> state{STATE_NEED_DATA};
        format6Type data; // 128 bytes ?
        string market;
        string filename;
        // int coreId; // Removed as requested
    };

    ThreadContext ctxA;
    ThreadContext ctxB;

    ctxA.market = marketA;
    ctxA.filename = "./data/" + marketA + "Quote." + dateA;
    // ctxA.coreId = 2; 

    ctxB.market = marketB;
    ctxB.filename = "./data/" + marketB + "Quote." + dateB;
    // ctxB.coreId = 3; 

    // 2. 定義讀檔 Worker (Lambda)
    // 這裡完整複製了你原本 readFile 的 parsing 邏輯 (Trade + Depth 合併)
    auto fileReaderWorker = [](ThreadContext* ctx, int core) { // Add core arg check
        // Pin Core
        pin_thread_to_core(core); // Use arg
        set_thread_name(("Reader_" + ctx->market).c_str());

        std::ifstream file;
        const size_t BUF_SIZE = 1024 * 1024;
        std::vector<char> buffer(BUF_SIZE);
        file.rdbuf()->pubsetbuf(buffer.data(), BUF_SIZE);

        file.open(ctx->filename);
        if (!file.is_open()) {
            errorLog("Open file fail: " + ctx->filename);
            ctx->state.store(STATE_FINISHED, std::memory_order_release);
            return;
        }

        std::string lineTrade, lineDepth, tmpLine;
        lineTrade.reserve(1024);
        lineDepth.reserve(1024);
        
        bool lineStored = false;

        while (true) {
            // A. 等待主執行緒通知 "我需要下一筆資料"
            // 使用 acquire 確保看到主執行緒的修改
            while (ctx->state.load(std::memory_order_acquire) == STATE_HAS_DATA) {
                _mm_pause(); // CPU hint: spin loop
            }

            // B. 讀取與解析邏輯 (與原本 readFile 相同)
            if (lineStored) {
                lineTrade = tmpLine;
            } else {
                if (!std::getline(file, lineTrade)) break; // EOF
            }
            lineStored = false;

            if (lineTrade.empty()) continue;
            // 簡易過濾
            if (lineTrade.size() < 2 || lineTrade[0] != 'T' || lineTrade[1] != 'r') continue;

            // 嘗試讀取下一行看是不是 Depth
            if (!std::getline(file, lineDepth)) lineDepth = "";

            if (lineDepth.size() >= 2 && lineDepth[0] == 'T' && lineDepth[1] == 'r') {
                tmpLine = lineDepth; // 其實是下一筆 Trade
                lineDepth = "";
                lineStored = true;
            }
            // 檢查 Trade 與 Depth 是否匹配 (例如時間戳記)
            if (lineDepth != "" && lineTrade.substr(5, 20) != lineDepth.substr(5, 20)) {
                lineDepth = "";
            }

            // C. 解析資料到 ctx->data
            ctx->data.market = ctx->market;
            // 這裡假設 fromRedis 會填寫 matchTimeStr
            if (ctx->data.fromRedis(lineTrade, lineDepth)) {
                if (ctx->data.statusCode == 0) {
                    // D. 解析成功，通知主執行緒
                    // 使用 release 確保 data 寫入完成後才改變 state
                    ctx->state.store(STATE_HAS_DATA, std::memory_order_release);
                } else {
                    // 解析失敗(statusCode!=0)，直接跳過讀下一筆，不需要通知主執行緒
                    continue; 
                }
            }
        }

        // E. 結束
        ctx->state.store(STATE_FINISHED, std::memory_order_release);
        file.close();
    };

    // 3. 啟動執行緒
    std::thread tA([&]() {
        // pin_thread_to_core(2); // In worker now, as requested "inside"
		// pin_thread_to_core(2);
		// set_thread_name("Reader_" + ctxA.market);
        fileReaderWorker(&ctxA, 2);
    });
    std::thread tB([&]() {
        // pin_thread_to_core(3);
		// set_thread_name("Reader_" + ctxB.market);
        fileReaderWorker(&ctxB, 3);
    });

    cout << "Start merging " << marketA << " and " << marketB << "...\n";
    
    // 4. 合併迴圈 (Main Thread)
    // long long readFileCnt = 0;
    
    // 為了效能，使用 local pointer
    format6Type* f6 = nullptr;
    int queueIdx = 0; // 統一寫入到 quoteQue[0]

    while (true) {
        // Step 1: 等待 A 準備好 (Ready 或 Finished)
        int sA = ctxA.state.load(std::memory_order_acquire);
        while (sA == STATE_NEED_DATA) {
             std::this_thread::yield(); 
            sA = ctxA.state.load(std::memory_order_acquire);
        }

        // Step 2: 等待 B 準備好
        int sB = ctxB.state.load(std::memory_order_acquire);
        while (sB == STATE_NEED_DATA) {
             std::this_thread::yield();
            sB = ctxB.state.load(std::memory_order_acquire);
        }

        // Step 3: 判斷結束條件
        if (sA == STATE_FINISHED && sB == STATE_FINISHED) {
            break;
        }

        // Step 4: 比較與寫入
        ThreadContext* winner = nullptr;

        if (sA == STATE_HAS_DATA && sB == STATE_HAS_DATA) {
            // 兩者都有資料，比較 matchTimeStr (字串比較)
            // 如果追求極致效能，建議在 fromRedis 轉成 long long matchTime_us 進行整數比較
            if (ctxA.data.matchTimeStr <= ctxB.data.matchTimeStr) {
                winner = &ctxA;
            } else {
                winner = &ctxB;
            }
        } else if (sA == STATE_HAS_DATA) {
            // 只有 A 有資料 (B 已結束)
            winner = &ctxA;
        } else if (sB == STATE_HAS_DATA) {
            // 只有 B 有資料 (A 已結束)
            winner = &ctxB;
        }

        // Step 5: 將勝出者的資料推入 Queue
        if (winner) {
            if (quoteQue[queueIdx]) {
                f6 = quoteQue[queueIdx]->alloc();
                *f6 = winner->data; // Copy Assignment
                f6->prevLimitUp = prevDayLimitUpMap[f6->symbol];
                f6->volatilityPause = numTracker.on_tick(f6->symbol, f6->matchTime_us) <= 3;
                
                quoteQue[queueIdx]->push();
				lastPrice[f6->symbol] = f6->match.Price;
                readFileCnt++;
                
            }
            
            // Step 6: 通知勝出者讀取下一筆 (Release lock)
            winner->state.store(STATE_NEED_DATA, std::memory_order_release);
        }
    }

    // 5. 等待執行緒結束
    if (tA.joinable()) tA.join();
    if (tB.joinable()) tB.join();
	readFileFinish = true;
    cout << "Merged Read Finish. Total count: " << readFileCnt << endl;
    fflush(stdout);
}