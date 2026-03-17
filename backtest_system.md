# 台股盤中動量回測系統 — 完整技術文件

> 最後更新：2026-03-16
> 分支：strategy-vwap-touch
> 基準 commit：bab2c3d (restore baseline)

---

## 目錄
1. [系統概述](#1-系統概述)
2. [執行流程](#2-執行流程)
3. [Signal A 策略邏輯](#3-signal-a-策略邏輯)
4. [StrongGroup 族群篩選](#4-stronggroup-族群篩選)
5. [Order 下單模組](#5-order-下單模組)
6. [資料流與效能](#6-資料流與效能)
7. [參數完整說明](#7-參數完整說明)
8. [回測結果與分析結論](#8-回測結果與分析結論)
9. [檔案清單與備份](#9-檔案清單與備份)
10. [災難復原指南](#10-災難復原指南)

---

## 1. 系統概述

### 核心策略
VWAP 近碰反彈策略（strategy-vwap-touch）：盤中監控強勢族群中的 M1 股票，當股價接近 VWAP 後反彈時進場做多，配合嚴格停損與分批停利。

### 關鍵數據
- **回測期間**：2025-11-21 至 2026-03-16（72 個交易日）
- **基準表現**（strict 參數）：~120 筆交易，PF ≈ 1.48，WR ≈ 43%
- **Adjusted PnL 公式**：`adj = PnL - 20,000 + (lockedLimitUp ? 120,000 : 0)`
  - 扣 20K 為滑價+手續費估計
  - 鎖漲停加 120K 為隔日開盤溢價估計

### 價格單位
所有價格以 `long long` 儲存，乘以 10,000（例如 50.00 元 → 500000）

### 時間格式
- `matchTimeStr`：整數格式，如 `91500000000` = 09:15:00.000000
- `matchTime_us`：微秒（用於 rolling window 計算）

---

## 2. 執行流程

### 編譯與執行
```bash
make              # 編譯 → build/HWStratSv → exec/sv/signal
make clean && make # 切分支後必須 clean rebuild
cd exec && ./sv/signal YYYYMMDD [logFolder]
```

### 啟動順序 (main.cpp)
```
1. pin_thread_to_core(1), SCHED_FIFO(99)
2. f1mgr.readFile(date)        → 載入前收盤價、漲跌停價
3. checkPrevDayLimitUp(date)   → 標記前日漲停股
4. getTickData("OTC", date)    → 載入 21 天歷史量（5 threads, cores 3-7）
5. getTickData("TSE", date)    → 同上
6. strongGroup.getGroup()      → 讀 group.csv，計算 symbol_is_valid
7. 建立 tickFilter            → 只保留有效族群成員 + 0050
8. readFileMerged()            → 雙線程讀 OTC+TSE，時間戳合併
   └→ StrategySv::run()       → 主策略迴圈（core 20, FIFO 89）
```

### 每筆 Tick 處理流程 (StrategySv::run)
```
for each tick:
  1. 0050 監控：開盤漲幅 → market_disabled 判斷
  2. IndexData 計算：VWAP、日高低、rolling_low
  3. strongGroup.on_tick() → 族群排名、M1 判定
  4. 決定 MatchType: None / StrongGroup
  5. signalA.eval() → 三階段模型判斷是否觸發
  6. if triggered: order.trigger() → 進場
  7. order.on_tick() → 停損/停利/超時出場
```

---

## 3. Signal A 策略邏輯

### 三階段模型

```
Phase 0: 前置條件檢查
  ├─ 時間在 [entry_start_time, entry_end_time)
  ├─ pre_condition_met(): 價格 > VWAP × 0.997（否則 forbidden = true，永久封鎖）
  └─ 漲幅 < 8.5%（trade_zone_max_increase_ratio）

Phase 1: 偵測接近 VWAP（從上方靠近）
  └─ price / VWAP ≤ vwap_near_ratio (1.007)
     → near_vwap = true, 記錄 low_since_near

Phase 2: 追蹤反彈
  ├─ 持續更新 low_since_near
  ├─ 超時檢查：near → entry 超過 300 秒則放棄
  └─ 反彈檢查：(price - low) / low ≥ bounce_ratio (0.008)
     → 觸發進場！
```

### 狀態機
```
forbidden:        一旦價格觸及 VWAP × 0.997，永久封鎖
near_vwap:        正在接近 VWAP 階段
low_since_near:   進入 near 後的最低價
near_vwap_time_us: 進入 near 的時間戳
triggered:        已觸發過（一天只觸發一次）
```

### MatchType 重置規則
- 當股票跌出族群篩選（matchType 變 None）→ 重置 near_vwap
- 這個「bug」反而是有效的門檻：暫時跌出族群的股票不應繼續追蹤

---

## 4. StrongGroup 族群篩選

### 三層過濾

#### Level 1: 個股有效性 (getGroup)
- 讀取 `group.csv`（族群名,股票代號,股票名稱）
- 20 天平均成交值 ≥ `member_min_month_trading_val` (200M)
- 結果存入 `symbol_is_valid`

#### Level 2: 族群有效性 (isValidGroup)
所有條件必須同時滿足：
1. 族群 20 天平均成交值合計 ≥ 3B
2. 族群平均漲幅 ≥ 1%
3. 當日成交值 / 月均 ≥ 1.2（確認正常交易）

#### Level 3: 成員排名 (on_tick)

**Raw ranking**（純排名，用於 M1 判定）：
- 包含所有月均成交值 ≥ 200M 的成員
- 排除鎖漲停、漲幅 ≥ 8.5%
- 按 VWAP 漲幅降序排列

**Active ranking**（篩選後排名，用於進場資格）：
- 額外條件：cond1（量增）、cond2（價量）、cond4（VWAP 漲幅 ≥ 3%）
- 目前 cond1/2/4 全部 disabled（最寬鬆模式）
- Top 20 族群各選 M1

### 進場資格判定
```
ans = true IF:
  ├─ member_rank ≤ max_select (1)
  ├─ entry_min_vwap_pct_chg (3.5%) ≤ VWAP漲幅 ≤ entry_max_vwap_pct_chg (6%)
  ├─ require_raw_m1 → raw_rank == 1
  ├─ !isPrevDayLimitUp（已啟用過濾）
  ├─ !blockDispositionEntry（未啟用）
  └─ entry_max_vol_ratio == 0（未啟用）
```

### last_match_info
每次符合條件時更新，記錄：group_name, group_rank, member_rank, raw_member_rank, m1_symbol, vol_ratio, month_trading_val

---

## 5. Order 下單模組

### 進場 (trigger)
```
前提條件：
  - 時間 < 13:00
  - 非前日漲停（filter_prev_day_limit_up=true）
  - 未持有同一股票
  - 進場價 ≤ 500 元

部位大小：
  stocks[symbol] = 10,000,000 / 進場價
```

### 停利 (takeProfit)
```
分 5 等份（take_profit_splits=2 + reserve_limit_up_splits=3）
  - TP 單 1: 進場價 × 1.03（掛單）
  - TP 單 2: 進場價 × 1.03（掛單）
  - Reserve 3 份：不掛單，等收盤處理
  - 所有 TP 價格不超過漲停價
```

### 停損 (stopLoss)
```
SignalA: price ≤ 進場時VWAP × 0.995
  → 全部出清（取消掛單 + 市價賣出 + reserve 歸零）
  → 加入 stoppedLossSymbols（不再進場）
```

### 回撤出場 (bailout)
```
條件：已有停利成交 AND price ≤ 進場時日高 × 0.8
  → 全部出清
```

### 超時出場 (timeExit, 13:20)
```
Case A: reserve > 0 且 鎖漲停
  → 非 reserve 部分市價賣出
  → reserve 以漲停價賣出
  → LeaveCause = "lockedLimitUp"

Case B: 其他
  → 全部市價賣出
  → LeaveCause = "timeExit"
```

### 報告欄位 (report_trades.csv, 26 欄)
```
Symbol, SignalType, EnterCause, EntryTime, ExitTime, LeaveCause,
PnL, Return%, HoldingDuration, GroupName, GroupRank, MemberRank,
RawMemberRank, M1Symbol, EntryPrice, EntryVWAP, DayHigh, PrevClose,
0050OpenChg%, VolRatio, MonthTradingVal, IsPrevDayLU, IsDisposition,
HadCircuitBreaker, GroupLimitUpCount, 0050EntryChg%
```

---

## 6. 資料流與效能

### 歷史量資料載入 (getTickData)
```
掃描 data/ 下的 [Type]Quote.YYYYMMDD
  → 從目標日期往回取 21 天
  → 5 個 worker threads 平行讀取
  → 建立 vol_cum[0..20] (LinearVolumeTracker)
  → 用於計算：月均同時段量、VolRatio
```

### Volume Cache 機制
```
首次讀取：解析原始檔 → 存為 .volcache（二進位格式）
後續讀取：直接載入 .volcache（秒級）
位置：data/[Type]Quote.YYYYMMDD.volcache
大幅加速 getTickData（從分鐘級降到秒級）
```

### tickFilter 優化
```
readFileMerged() 中：
  - 只處理 tickFilter 集合中的 symbol（~380/1787）
  - 跳過不在集合中的 tick → 減少 ~78% 的 tick 處理量
  - 已驗證與無過濾版結果完全一致
```

### 執行速度
- 單日回測（含 tickFilter）：~10-12 秒
- 單日回測（無 tickFilter）：~15-20 秒
- 72 天 batch：~12-15 分鐘（含 tickFilter）

### 執行緒模型
```
Core 1:   main（初始化）
Core 3-7: getTickData workers (5 threads)
Core 11:  readFileMerged readerA (OTC)
Core 12:  readFileMerged readerB (TSE)
Core 20:  StrategySv::run (主策略迴圈)
```

---

## 7. 參數完整說明

### [Strategy]
| 參數 | 值 | 說明 |
|------|------|------|
| market_rally_disable_threshold | 0.2 | 0050 在 09:15 漲幅 ≥ 20% → 停止交易 |
| market_open_min_chg | -0.1 | 0050 開盤跌幅 > 10% → 停止交易 |

### [SignalA]
| 參數 | 值 | 說明 |
|------|------|------|
| vwap_near_ratio | 1.007 | price/VWAP ≤ 此值 → 進入 near 階段 |
| bounce_ratio | 0.008 | 從 near 低點反彈 0.8% → 觸發進場 |
| entry_start_time | 90400000000 | 最早進場時間 09:04 |
| entry_end_time | 92500000000 | 最晚進場時間 09:25 |
| pre_condition_start_time | 90400000000 | forbidden 邏輯啟動時間 |
| pre_condition_vwap_ratio | 0.997 | price ≤ VWAP × 0.997 → forbidden |
| trade_zone_max_increase_ratio | 0.085 | 漲幅 > 8.5% → 排除 |
| max_near_to_entry_sec | 300 | near → entry 超過 300 秒 → 放棄 |

### [StrongGroup]
| 參數 | 值 | 說明 |
|------|------|------|
| member_min_month_trading_val | 200000000 | 個股月均成交值門檻 200M |
| group_min_month_trading_val | 3000000000 | 族群月均成交值門檻 3B |
| group_min_avg_pct_chg | 0.01 | 族群平均漲幅門檻 1% |
| group_min_val_ratio | 1.2 | 當日/月均成交值比 ≥ 1.2 |
| member_strong_vol_ratio | 1.5 | 量增比門檻 1.5x（cond1，目前停用）|
| member_strong_trading_val | 2000000000 | 大戶豁免門檻 2B（cond1）|
| top_group_rank_threshold | 10 | 前 10 名族群為 top group |
| top/normal_group_max/min_select | 1 | 每個族群選 1 名 M1 |
| member_vwap_pct_chg_threshold | 0.03 | 成員 VWAP 漲幅 ≥ 3%（cond4，停用）|
| group_valid_top_n | 20 | 只有前 20 名族群可出場 |
| is_weighted_avg | false | 族群漲幅用等權平均 |
| group_vol_ratio_exempt_threshold | 30B | 超大族群豁免量條件 |
| exclude_prev_limit_up_from_rank | false | 前日漲停股參與排名 |
| exclude_disposition_from_rank | false | 處置股參與排名 |
| member_cond1/2/4_enabled | false | 成員條件全部停用 |
| entry_min_vwap_pct_chg | 0.035 | 進場 VWAP 漲幅下限 3.5% |
| entry_max_vwap_pct_chg | 0.06 | 進場 VWAP 漲幅上限 6% |
| require_raw_m1 | true | 必須是 raw ranking 第 1 名 |
| block_disposition_entry | false | 不擋處置股進場 |
| entry_max_vol_ratio | 0 | 量增比上限（0=不限）|
| entry_min_group_rank | 0 | 族群排名下限（0=不限）|

### [Order]
| 參數 | 值 | 說明 |
|------|------|------|
| position_cash | 10000000 | 每筆部位 10M |
| filter_prev_day_limit_up | true | 過濾前日漲停 |
| disposition_stocks_enabled | true | 允許處置股交易 |
| stop_loss_ratio_a | 0.995 | SignalA 停損：VWAP × 0.995 |
| stop_loss_ratio_b | 0.993 | SignalB 停損：rolling_low × 0.993 |
| bailout_ratio | 0.8 | 回撤出場：日高 × 0.8 |
| entry_time_limit | 130000000000 | 進場截止 13:00 |
| exit_time_limit | 132000000000 | 出場截止 13:20 |
| take_profit_splits | 2 | 停利分 2 批 |
| take_profit_pcts | 0.03,0.03 | 各 3% 停利 |
| reserve_limit_up_splits | 3 | 保留 3 份等漲停 |
| tp_base_entry | true | 停利基準用進場價 |
| max_entry_price | 500 | 最高進場價 500 元 |

---

## 8. 回測結果與分析結論

### 基準表現（72 天 strict，852e3dd group.csv）

| 價格區間 | 筆數 | 勝率 | PF | AdjPnL |
|----------|------|------|-----|--------|
| ≤50 | 26 | 46.2% | 1.79 | +1,881K |
| 50-100 | 40 | 40.0% | 1.32 | +1,370K |
| 100-200 | 32 | 43.8% | 1.50 | +1,662K |
| 200-300 | 17 | 47.1% | 1.47 | +699K |
| 300-500 | 5 | 40.0% | 1.25 | +123K |
| **≤500** | **120** | **43.3%** | **1.48** | **+5,735K** |

### 已驗證的分析結論（68天 batch_tagged68）

#### 前日漲停：必須過濾
- 非漲停 92 筆 PF=1.82 +6,827K
- 漲停 18 筆 PF=0.61 -987K

#### 處置股：不擋
- 6 筆 100% 勝率，全部鎖漲停，+2,295K

#### 緩搓（CircuitBreaker）：不過濾
- 38 筆 PF=1.93 +3,442K（比非緩搓更好）

#### VWAP 漲幅甜蜜點
- 3.5-6% 最佳

#### 量增比 (VolRatio)
- <1x 最佳 PF=5.20（但筆數少）
- <8x 合理門檻 PF=1.98
- ≥8x 很差 PF=0.66

#### 進場時間
- 09:04 直接進場完勝，near→entry 5min+ 是災難

#### max_near_to_entry_sec = 300
- 5 分鐘超時合理

---

## 9. 檔案清單與備份

### 核心程式碼（git tracked）
```
main.cpp                    # 程式入口
Order/Order.cpp, .h         # 下單模組
StrategySv/StrategySv.cpp, .h   # 策略主迴圈
StrategySv/signalA.cpp, .h      # Signal A
StrategySv/strongGroup.cpp, .h  # 族群篩選
QuoteSv/QuoteSv.cpp, .h        # 行情服務
exec/cfg/parameter.cfg         # 策略參數
exec/files/group.csv           # 族群定義（852e3dd 版, 2085 行）
```

### 回測資料（exec/）
```
batch_baseline/          # 當前基準 batch（tickFilter 啟用）
batch_0313_oldgroup/     # 無 tickFilter 參考版
batch_vwaptouch_relaxed/ # 放寬參數版（391 筆）
old_batches/             # 歸檔的舊 batch
```

### 分析腳本（exec/）
```
analyze_params.py        # VWAP%/rawM1/大盤切點掃描
analyze_duration.py      # near→entry 時間分析
analyze_tags.py          # 前日漲停/處置股/緩搓分析
analyze_sweep.py         # forbidden sweep 分析
simulate_exit.py         # 離線停損停利模擬
equity_curve.py          # 權益曲線
cross_analysis.py        # 交叉分析
```

### 重要備份檔
```
exec/cfg/parameter.cfg.strict_backup   # strict 參數備份
exec/cfg/parameter.cfg.user_backup     # 放寬參數備份
exec/files/group.csv.old              # 852e3dd 版 group.csv
exec/files/group.csv.current          # 0220 版 group.csv（舊版）
```

---

## 10. 災難復原指南

### 如果 group.csv 被覆蓋
```bash
git show 852e3dd:exec/files/group.csv > exec/files/group.csv
# 或
cp exec/files/group.csv.old exec/files/group.csv
```

### 如果 parameter.cfg 被改壞
```bash
git show 852e3dd:exec/cfg/parameter.cfg > exec/cfg/parameter.cfg
# 注意：852e3dd 的 market_open_min_chg=-0.1，
#       但不影響結果（所有交易日 0050 開盤跌幅 < 0.1%）
```

### 如果回測結果不一致
檢查清單（按影響大小排序）：
1. **group.csv 版本** — 影響最大，決定哪些股票被選中
2. **parameter.cfg** — 特別注意 entry_min/max_vwap_pct_chg, require_raw_m1
3. **binary 是否重編** — `make clean && make`
4. **tickFilter 狀態** — 應啟用，已驗證不影響結果
5. **roundUpToTick** — 應關閉（目前已 revert）

### 切分支標準流程
```bash
# 切之前
git add -A && git commit -m "save state"
git push

# 切分支
git checkout <branch>

# 切之後（必須！）
make clean && make
```

### 驗證回測正確性
```bash
# 跑單日，比對已知結果
cd exec && ./sv/signal 20260107
# 預期：6 筆交易，PnL 合計 ~1,036K
# 3037: -144K, 2303: +374K, 1303: +381K,
# 6217: +194K, 6244: +345K, 1605: +388K
```
