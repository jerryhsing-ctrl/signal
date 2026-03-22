/*
 * 格式六：集中市場普通股競價交易即時行情資訊 
 */
//---------------------------------------------------------------------------
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "time.h"
#include "PackBCD.h"
#include "Format6.h"
#include <bitset>
#include <cstring>
#include "errorLog.h"
#include <climits>


void Format6::init(unsigned char *package_src, int length) 
{
    totalBidQty = 0; 
    totalAskQty = 0; 
    tradeAt = 0; 
    isLimitUpLocked = false; 
    isLimitDownLocked = false;
    match = {};
    memset(bid, 0, sizeof(bid));
    memset(ask, 0, sizeof(ask));

    const unsigned char* cursor = package_src + 10;
    const Format6Body* body = reinterpret_cast<const Format6Body*>(cursor);
    cursor += sizeof(Format6Body); // 指標滑動
    symbol = "";
	for (unsigned char c : body->ProdID) {
        if ((char) c == ' ')
            continue;
		symbol += (char) c;
	}

    
    *reinterpret_cast<uint32_t*>(matchTime)     = *reinterpret_cast<const uint32_t*>(body->MatchTime);
    *reinterpret_cast<uint16_t*>(matchTime + 4) = *reinterpret_cast<const uint16_t*>(body->MatchTime + 4);

    int hour = ((matchTime[0] >> 4) & 0x0F) * 10 + (matchTime[0] & 0x0F);
    int min  = ((matchTime[1] >> 4) & 0x0F) * 10 + (matchTime[1] & 0x0F);
    int sec  = ((matchTime[2] >> 4) & 0x0F) * 10 + (matchTime[2] & 0x0F);
    int msec = ((matchTime[3] >> 4) & 0x0F) * 100 + ((matchTime[3] & 0x0F)) * 10 + ((matchTime[4] >> 4) & 0x0F);
    int usec = ((matchTime[4] & 0x0F)) * 100 + ((matchTime[5] >> 4) & 0x0F) * 10 + ((matchTime[5] & 0x0F));
    matchTime_us = (long long) hour * 3600 * 1000000 + (long long) min  * 60 * 1000000 + (long long) sec * 1000000 + (long long)msec * 1000 + (long long)usec;
                      
	totalMatchQty = PackBCD4(body->TotalMatchQty);

    unsigned char dItem = body->DisplayItem;
    tradeCode = (dItem >> 7) & 1;           // bit 7
    bestCode  = dItem & 1;                  // bit 0
    bidCount  = (dItem >> 4) & 0x07;        // bits 4-6
    askCount  = (dItem >> 1) & 0x07;        // bits 1-3

    statusCode = (body->StatusItem >> 7) & 1;

    int quotePrice;
    
    if (tradeCode == 1) 
    {            
        const Format6Quote* matchQuote = reinterpret_cast<const Format6Quote*>(cursor);
        cursor += sizeof(Format6Quote); // 指標滑動

        // quotePrice = PackBCD2Int(matchQuote->Price, sizeof(matchQuote->Price));
		quotePrice = PackBCD5(matchQuote->Price);
        match.Price = quotePrice; // 乘法取代除法
		// quotePrice = PackBCD5(matchQuote->Price);
        // match.Qty   = PackBCD2Int(matchQuote->Qty, sizeof(matchQuote->Qty));     
		match.Qty = PackBCD4(matchQuote->Qty);     
    }    

    // 6. 快速檢核
    if (bestCode != 0) return;
    if ((bidCount | askCount) == 0) return; // OR 運算判斷全零
    
    // 7. 委買迴圈 (Bid)
    for (int i = 0; i < bidCount; ++i)
    {
        const Format6Quote* depth = reinterpret_cast<const Format6Quote*>(cursor);
        cursor += sizeof(Format6Quote); // 指標滑動
        
        // quotePrice  = PackBCD2Int(depth->Price, sizeof(depth->Price));
		quotePrice = PackBCD5(depth->Price);
		bid[i].Price = quotePrice;

        // bid[i].Qty  = PackBCD2Int(depth->Qty, sizeof(depth->Qty));
        bid[i].Qty  = PackBCD4(depth->Qty);
        
        totalBidQty += bid[i].Qty;
    }

    // 8. 委賣迴圈 (Ask)
    for (int i = 0; i < askCount; ++i)
    {
        const Format6Quote* depth = reinterpret_cast<const Format6Quote*>(cursor);
        cursor += sizeof(Format6Quote); // 指標滑動

        // quotePrice  = PackBCD2Int(depth->Price, sizeof(depth->Price));
		quotePrice = PackBCD5(depth->Price);
		ask[i].Price = quotePrice;

        // ask[i].Qty  = PackBCD2Int(depth->Qty, sizeof(depth->Qty));
        ask[i].Qty = PackBCD4(depth->Qty);
        
        totalAskQty += ask[i].Qty;
    }



    unsigned char limitDownUpBit = body->RiseFallItem;
    isLimitUpLocked = ((limitDownUpBit >> 4) & 0x03) == 2 && totalAskQty == 0;  // 漲停且無委賣量
    isLimitDownLocked = ((limitDownUpBit >> 2) & 0x03) == 1 && totalBidQty == 0;  // 跌停且無委買量

    // 一般: 成交在買價是內盤, 成交在賣價是外盤. 有巿價的話反過來, 漲停巿價是外盤, 跌停巿價是內盤
    if (tradeCode == 1) 
    {
        if (bidCount > 0 && match.Price == bid[0].Price) 
            tradeAt = 1; 
        else if (askCount > 0 && match.Price == ask[0].Price)
            tradeAt = 2;      
        
        if (((limitDownUpBit >> 4) & 0x03) == 2) { // 漲停買進
            // cout << " 漲停買進 " << bidCount << '\n';
            if (bidCount < 1)
                errorLog("漲停買進但沒有最佳一檔委買");
            if (bid[0].Price == 0) // 漲停買進且委買一是市價時是
                tradeAt = 2;
            
        }
        else if (((limitDownUpBit >> 2) & 0x03) == 1) {
            // cout << " 跌停賣出 " << askCount << '\n';
            if (askCount < 1)
                errorLog("跌停賣出但沒有罪加一檔委賣");
            if (ask[0].Price == 0)
                tradeAt = 1; 
        }
    }   
}
//---------------------------------------------------------------------------

long long convertRawTimeToMicroseconds(long long rawTime) {
    // 1. 拆解時間單位
    // 取出後 6 位 (微秒)
    long long micros = rawTime % 1000000;
    
    // 去掉微秒部分，剩下 HMMSS
    long long remaining = rawTime / 1000000;
    
    // 取出後 2 位 (秒)
    long long seconds = remaining % 100;
    remaining /= 100; // 去掉秒
    
    // 取出後 2 位 (分)
    long long minutes = remaining % 100;
    
    // 剩下的就是小時
    long long hours = remaining / 100;
    
    // 2. 計算總微秒數
    // 先算總秒數：(時*3600) + (分*60) + 秒
    long long totalSeconds = (hours * 3600) + (minutes * 60) + seconds;
    
    // 最後轉換為微秒並加上原本的零頭
    return (totalSeconds * 1000000LL) + micros;
}

std::pair<long long, long long> getBestPrices(const std::string& line) {
    long long bidPrice = 0;
    long long askPrice = 0;

    // --- 處理 BID (委買) ---
    size_t bidPos = line.find("BID:");
    if (bidPos != std::string::npos) {
        // 1. 先讀 "BID:" 後面的數字 (檔數)
        // stoi 會讀到逗號自動停
        int count = std::stoi(line.substr(bidPos + 4)); 

        if (count > 0) {
            // 2. 如果檔數 > 0，找到後面的逗號，逗號後就是價格
            size_t priceStart = line.find(',', bidPos);
            // stoll 會讀到乘號 '*' 自動停
            bidPrice = std::stoll(line.substr(priceStart + 1));
        }
    }

    // --- 處理 ASK (委賣) ---
    size_t askPos = line.find("ASK:");
    if (askPos != std::string::npos) {
        // 1. 先讀 "ASK:" 後面的數字
        int count = std::stoi(line.substr(askPos + 4));

        if (count > 0) {
            // 2. 如果檔數 > 0，找到後面的逗號，讀取價格
            size_t priceStart = line.find(',', askPos);
            askPrice = std::stoll(line.substr(priceStart + 1));
        }
    }

    return {bidPrice, askPrice};
}

bool Format6::fromRedis(const std::string& data, const std::string& depth) {
    // memset(this, 0, sizeof(Format6)); // 清空整個物件，避免殘留數據影響解析
    match = {};
    memset(bid, 0, sizeof(bid));
    memset(ask, 0, sizeof(ask));
    tradeAt = 0;

    const char* p = data.c_str();

    // Trade
    bool isTrade = (strncmp(p, "Trade", 5) == 0);
    if (!isTrade) {
        errorLog(" not trade");
        return false;
    }
    while (*p && *p != ',') p++;
    if (*p == ',') p++;
    else errorLog("Format6::fromRedis, isTrade not found");

    // 股票代碼
    // int s_len = 0;
    symbol = "";
    while (*p && *p != ',') {
        if (*p != ' ')
            symbol += *p;
        p++;
    }
    if (*p == ',') p++;
    else errorLog("Format6::fromRedis, symbol not found");

    // 3. matchTime_us (83008816030)
    matchTimeStr = 0;
    while (*p >= '0' && *p <= '9') {
        matchTimeStr = matchTimeStr * 10 + (*p - '0');
        p++;
    }
    matchTime_us = convertRawTimeToMicroseconds(matchTimeStr);
    if (*p == ',') p++;
    else errorLog("Format6::fromRedis, matchTime_us not found");

    // 4. tradeCode (1)
    statusCode = 0;
    if (*p == '0') statusCode = 0;
    else if (*p == '1') statusCode = 1;
    else errorLog("Format6::fromRedis, statusCode not valid");
    p++;

    if (*p == ',') p++;
    else errorLog("Format6::fromRedis, statusCode not found");

    // 5. price (941000)
    match.Price = 0;
    while (*p >= '0' && *p <= '9') {
        match.Price = match.Price * 10 + (*p - '0');
        p++;
    }
    if (*p == ',') p++;
    else errorLog("Format6::fromRedis, match.Price not found");

    // 6. qty (163)
    match.Qty = 0;
    while (*p >= '0' && *p <= '9') {
        match.Qty = match.Qty * 10 + (*p - '0');
        p++;
    }
    if (*p == ',') p++;
    else errorLog("Format6::fromRedis, match.Qty not found");

    if (!depth.empty()) {
        pair<long long, long long> bidAsk = getBestPrices(depth);

        if (match.Price == bidAsk.first)
            tradeAt = 1;
        else 
            tradeAt = 2;
        bid[0].Price = bidAsk.first;
        ask[0].Price = bidAsk.second;
    }
    else {
        tradeAt = 0;
    }
    
    // cout << " trade " << trade << '\n';
    // cout << "       bid " << bidAsk.first << " ask " << bidAsk.second << " tradeAt " << tradeAt << '\n';
    // cout << "Format6::fromRedis, isTrade: [" << isTrade << "], symbol: [" << symbol << "], matchTime_us: [" << matchTime_us << "], statusCode: [" << statusCode << "], match.Price: [" << match.Price << "], match.Qty: [" << match.Qty << "]" << endl;
    tradeCode = 1;
    // tradeAt = 1;
    return true;
}




Format6::~Format6()
{
}
//---------------------------------------------------------------------------



void Format6::record(std::string seqno) {
    std::string fileName = "./log/Format6.log";
	std::ofstream ofs(fileName, std::ios::app);

    std::stringstream oss;
    oss << "========================================\n";
    oss << "SeqNo: [" << seqno << "]\n";
    oss << "Time: [" << now_hms_nano() << "]\n";
    oss << "Symbol: [" << symbol << "]\n";

    oss << "TradeCode: [" << tradeCode << "]\n";
    oss << "BestCode: [" << bestCode << "]\n";
    oss << "BidCount: [" << bidCount << "]\n";
    oss << "AskCount: [" << askCount << "]\n";
    oss << "StatusCode: [" << statusCode << "]\n";

    oss << "MatchTime(BCD): [";
    for (int i = 0; i < 6; i++) {
        oss << (int)matchTime[i] << " ";
    }
    oss << "]\n";
    
    oss << "Match Price: [" << match.Price << "], Qty: [" << match.Qty << "]\n";
    
    oss << "--- Bids ---\n";
    for(int i=0; i<5; i++) {
        oss << "Bid[" << i << "] Price: [" << bid[i].Price << "], Qty: [" << bid[i].Qty << "]\n";
    }
    
    oss << "--- Asks ---\n";
    for(int i=0; i<5; i++) {
        oss << "Ask[" << i << "] Price: [" << ask[i].Price << "], Qty: [" << ask[i].Qty << "]\n";
    }

    oss << "TotalMatchQty: [" << totalMatchQty << "]\n";
    oss << "TotalBidQty: [" << totalBidQty << "]\n";
    oss << "TotalAskQty: [" << totalAskQty << "]\n";
    
    oss << "TradeAt: [" << tradeAt << "]\n";
    oss << "isLimitUpLocked: [" << (isLimitUpLocked ? "true" : "false") << "]\n";
    oss << "isLimitDownLocked: [" << (isLimitDownLocked ? "true" : "false") << "]\n";
    oss << "========================================\n";

    std::string output = oss.str();
    if (ofs.is_open()) {
        ofs << output;
        ofs.close();
    }
    else {
        errorLog("開檔失敗");
    }
}
