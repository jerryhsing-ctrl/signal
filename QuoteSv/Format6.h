/*
 * 格式六：集中市場普通股競價交易即時行情資訊 
 */
//---------------------------------------------------------------------------
#ifndef Format6_H_
#define Format6_H_
//---------------------------------------------------------------------------
// #include "TwseQuoteBase.h"
#include <chrono>
#include <string>
#include "time.h"
#include "pool.h"

using namespace std;
//-------------------------------------------------------------------------------------------------------------------------
#pragma pack(1)



struct Format6Body
{
	unsigned char ProdID[6];			//股票代號 X(06) 
	unsigned char MatchTime[6];			//撮合時間  9(12)   
	unsigned char DisplayItem;			//揭示項目註記   0︰無成交價、成交量，不傳送 1︰有成交價、成交量，而且傳送 
	unsigned char RiseFallItem;			//漲跌停註記       00：一般成交 01：跌停成交   10：漲停成交 
	unsigned char StatusItem;			//狀態註記             0：一般揭示 1：試算揭示 
	unsigned char TotalMatchQty[4];		//累計成交量 9(08) 
};
struct Format6Quote
{
    unsigned char Price[5];				//價格, 9(05)V9(4) 
    unsigned char Qty[4];				//數量, 9(08)
};
struct QuotePair
{
    int Price{0}; 				//價格, 9(05)V9(4) 
    int Qty{0}; 					//數量, 9(08)	
};
#pragma pack()
//-------------------------------------------------------------------------------------------------------------------------
class Format6: public Poolable
{
	public:
		Format6() = default;              // 無參數建構子
        // Format6(unsigned char *package_src, int length);
		void init(unsigned char *package_src, int length);

		bool fromRedis(const std::string& trade, const std::string& depth);

        virtual ~Format6();

		string symbol;
		short tradeCode;  // 0︰無成交價、成交量，不傳送        1︰有成交價、成交量，而且傳送 
		short bestCode;   // 0︰揭示成交價量與最佳五檔買賣價量    1︰僅揭示成交價量、不揭示最佳五檔買賣價量 
		short bidCount;   // 揭示買進價、買進量之傳送之檔位數（一至五檔 以二進位 BINARY 表示
		short askCount;   // 揭示賣出價、賣出量之傳送之檔位數（一至五檔 以二進位 BINARY 表示
		short statusCode;  // 試算狀態註記 0：一般揭示 1：試算揭示 	
        char matchTime[6];  
		
        QuotePair match;	  //成交
        QuotePair bid[5];     //委買五檔   			
        QuotePair ask[5];     //委賣五檔   			
	   
		long long matchTime_us;
		long long matchTimeStr;
		int  totalMatchQty, totalBidQty, totalAskQty;
		short tradeAt; //0,不是內盤也不是外盤  內盤:1, 成交在買價，表示賣方急於成交, 外盤:2, 成交在賣價，表示買方急於成交
		bool  isLimitUpLocked;  // 漲停鎖死
		bool  isLimitDownLocked;  // 跌停鎖死

		timeType time;
		void record(std::string seqno);
};
//---------------------------------------------------------------------------
#endif /* Format6_H_ */
