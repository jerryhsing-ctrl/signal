#pragma once

#include <array>    // 用於 std::array
#include <string>   // 用於 std::string
#include <cstddef>  // 用於 size_t (template<size_t N> 中需要)
#include "errorLog.h"
struct SecInfo {
    char category;   // '1' ~ '7'
    double tick;
    bool isStock;
    bool isETF;
    bool isWarrant;
    bool isCB;
    bool isCorpBond;
    bool isForeignBond;
    bool isGovBond;
};

// ============================================================
// 1. 7 類別分類器（你已確認正確）
// ============================================================

constexpr char Init4DigitCategory(int code)
{
    if (code >= 300 && code <= 899) return '2';      // Warrant
    if (code <= 199) return '4';                     // ETF 領域
    if (code >= 9100 && code <= 9199) return '5';    // Corporate Bond
    if (code < 100) return '7';                      // Government Bond head
    if (code >= 1000 && code <= 9999) return '1';    // Stock
    return '1';
}

constexpr auto MakeCategoryTable()
{
    std::array<char,10000> t{};
    for (int i=0;i<10000;i++)
        t[i] = Init4DigitCategory(i);
    return t;
}

constexpr auto CAT4 = MakeCategoryTable();

inline char GetCategory(const std::string& id)
{
    const int n = id.size();
    if (n < 4) return '1';

    const int code =
        (id[0]-'0')*1000 +
        (id[1]-'0')*100 +
        (id[2]-'0')*10 +
        (id[3]-'0');

    char cat = CAT4[code];

    char C5 = (n>5 ? id[5] : 0);
    char C6 = (n>6 ? id[6] : 0);

    // 外國債：F 開頭
    if (id[0]=='F') return '6';

    // 公債
    if (cat=='7') return '7';

    // 權證
    if (code>=300 && code<=899 &&
        C5>='0' && C5<='9' &&
        ((C6>='0'&&C6<='9') || C6=='P'||C6=='F'||C6=='Q'||C6=='C'||C6=='B'||C6=='X'||C6=='Y'))
        return '2';

    // 可轉債
    if (code>=1000 && code<=9999 && (C5>='1' && C5<='9'))
        return '3';

    // 附認股權公司債
    if (C5=='G' && (C6>='D' && C6<='L')) return '3';
    if (C5=='F' && (C6>='1' && C6<='9')) return '3';
    if (C5>='L' && C5<='Z') return '3';
    if (C5=='0' && (C6>='1' && C6<='9')) return '3';

    // Corporate Bond
    if (code>=9100 && code<=9199) return '5';

    // ETF
    if (cat=='4') return '4';

    // 預設股票
    return '1';
}

struct TickRule {
    double price;
    double tick;
};

// Type 1 rule (股票)
constexpr std::array<TickRule, 6> Rule1 {{
    {1000.0, 5.0},
    {500.0, 1.0},
    {100.0, 0.5},
    {50.0, 0.1},
    {10.0, 0.05},
    {0.0, 0.01},
}};

// Type 2 rule (權證)
constexpr std::array<TickRule, 6> Rule2 {{
    {500.0, 5.0},
    {150.0, 1.0},
    {50.0, 0.5},
    {10.0, 0.1},
    {5.0, 0.05},
    {0.0, 0.01},
}};

// Type 3 (可轉債)
constexpr std::array<TickRule, 3> Rule3 {{
    {1000.0, 5.0},
    {150.0, 1.0},
    {0.0,   0.05}
}};

// Type 4 ETF
constexpr std::array<TickRule, 2> Rule4 {{
    {50.0, 0.05},
    {0.0,  0.01}
}};

// Type 5 債券
constexpr std::array<TickRule, 1> Rule5 {{
    {0.0, 0.05}
}};


/// ====== 核心：用區間查表 ======
template<size_t N>
inline constexpr double fastTickLookup(const std::array<TickRule, N>& rules, double price)
{
    for (const auto& r : rules)
        if (price >= r.price)
            return r.tick;
    return rules.back().tick;
}


/// ====== 對外公開的最快 GetTick ======

inline double GetTick(const std::string& symbol, double price, bool isUp)
{	
	char stockType = GetCategory(symbol);

    if (!isUp) {
        price -= 0.0001;
    }

    switch (stockType)
    {
        case '1': return fastTickLookup(Rule1, price);
        case '2': return fastTickLookup(Rule2, price);
        case '3': return fastTickLookup(Rule3, price);
        case '4': return fastTickLookup(Rule4, price);
        case '5': return fastTickLookup(Rule5, price);
    }
    return 0.01;
}
// inline double GetTick(const std::string& symbol, double price)
// {	
// 	char stockType = GetCategory(symbol);
//     switch (stockType)
//     {
//         case '1': return fastTickLookup(Rule1, price);
//         case '2': return fastTickLookup(Rule2, price);
//         case '3': return fastTickLookup(Rule3, price);
//         case '4': return fastTickLookup(Rule4, price);
//         case '5': return fastTickLookup(Rule5, price);
//     }
//     return 0.01;
// }



inline int64_t getPriceCond(const string &symbol, int64_t price, int tick) {
    if (symbol.empty()) {
        errorLog("getPriceCond: symbol is empty");
        return price;
    }
    #define CHG 10000
    if (tick == 0)
        return price;

    double price_double = price;
    price_double /= CHG;
    // cout << "getPriceCond: initial price_double: " << price_double << " tick " << tick << '\n';
    if (tick > 0) {
        for (int i = 0; i < tick; i++) {
            price_double += GetTick(symbol, price_double, true);
            // cout << "getPriceCond: tick +" << i << ", price_double: " << price_double << " tick: " << GetTick(symbol, price_double, true) << '\n';
        }
    }
    else if (tick < 0) {
        for (int i = 0; i < abs(tick); i++) {
            price_double -= GetTick(symbol, price_double, false);
        }
    }
    // cout << " price double after getPriceCond: " << price_double << " double " << price_double * CHG << " int64 " << ((int64_t)(price_double * CHG + 0.01)) << '\n';
    return (int64_t)(price_double * CHG + 0.01);
}


