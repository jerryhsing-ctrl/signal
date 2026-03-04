# 功能詳細文件

---

## 1. 系統架構與資料流

### 執行流程
```
main()
  ├── QuoteSv::f1mgr.readFile()         ← 讀取前收盤、漲跌停資訊
  ├── QuoteSv::checkPrevDayLimitUp()     ← 標記前日漲停股
  ├── QuoteSv::getTickData() × 2        ← 多執行緒載入 20 天歷史逐筆量資料
  ├── StrongGroup::getGroup()            ← 計算族群月均成交金額
  └── QuoteSv::readFileMerged()          ← 雙市場行情合併讀取 → SPSC 佇列
                                                │
                                    StrategySv::run() [Core 20, FIFO-99]
                                                │
                                    ┌─────────────────────────────┐
                                    │  每筆 tick（Format6）：       │
                                    │  1. indexCalc.calc()         │
                                    │     → VWAP、日高、日低        │
                                    │  2. order.on_tick()          │
                                    │     → 停損/停利/出場檢查      │
                                    │  3. strongSingle.on_tick()   │
                                    │     → 強勢個股判定            │
                                    │  4. strongGroup.on_tick()    │
                                    │     → 強勢族群判定            │
                                    │  5. signalA.eval() / B.eval()│
                                    │     → 訊號三階段狀態機        │
                                    │  6. order.trigger()          │
                                    │     → 進場下單               │
                                    └─────────────────────────────┘
                                                │
                                    order.generateReport()  ← 檔案結束時
```

### 關鍵資料結構

**IndexData**（每支股票即時計算）
| 欄位 | 型別 | 說明 |
|---|---|---|
| `vwap` | `double` | 成交量加權平均價（整數價格 ÷ 10000） |
| `rolling_low` | `long long` | 5 分鐘滑動最低價（由 signalA/B 填入） |
| `day_high` | `long long` | 當日最高價 |
| `day_low` | `long long` | 當日最低價 |

**Format6**（每筆 tick 資料）
| 欄位 | 說明 |
|---|---|
| `symbol` | 股票代號 |
| `match.Price / Qty` | 成交價（×10000）/ 成交量 |
| `bid[5] / ask[5]` | 五檔買賣價量 |
| `matchTime_us` | 成交時間（微秒） |
| `matchTimeStr` | 成交時間（整數格式，如 `91500000000` = 09:15:00） |
| `tradeAt` | 0=無/1=內盤（買方掛單成交）/2=外盤（賣方掛單成交） |
| `isLimitUpLocked` | 是否鎖漲停 |
| `prevLimitUp` | 是否前日漲停 |
| `volatilityPause` | 是否處暫停交易 |

---

## 2. Signal A — 碰均線量縮反彈訊號

### 核心邏輯
股價跌至均線（rolling_low）附近，確認量縮後，等待反彈突破進場。

### 三階段狀態機
```
追蹤區 (Track Zone)  →  緩衝區 (Buffer Zone)  →  交易區 (Trade Zone)  →  觸發進場
```

#### 階段 1：追蹤區 → 進入緩衝區
**觸發條件**（`track_zone_eval`）：
- `price <= VWAP × track_zone_vwap_ratio`（價格靠近均線）
- `price <= rolling_low`（創 5 分鐘新低）
- 需要有 StrongSingle 或 StrongGroup 匹配（`matchType != None`）

#### 階段 2：緩衝區
**退出條件**（`buffer_zone_exit`，任一滿足即退出）：
- 停留超過 `buffer_zone_duration_min` 分鐘
- `price >= VWAP × buffer_zone_exit_vwap_ratio`（價格反彈太高）

**推進條件**（`buffer_zone_eval`，全部滿足才推進到交易區）：
- 時間在 `[buffer_zone_start_time, buffer_zone_end_time)` 內
- `rolling_sum_ratio < vol_contract_ratio`（短期量 / 長期量 < 閾值 → 量縮確認）
- `price <= day_high × buffer_zone_day_high_ratio`（價格未回到日高附近）
- `price >= VWAP × buffer_zone_vwap_entry_ratio`（價格不能跌太深）

#### 階段 3：交易區
**退出條件**（`trade_zone_exit`，任一滿足即退出）：
- 停留超過 `trade_zone_duration_min` 分鐘
- `price <= trigger_price × trade_zone_exit_price_ratio`（價格繼續下跌）

**觸發進場**（`trade_zone_eval`，全部滿足）：
- 時間 >= `trade_zone_start_time`
- `price >= max(rolling_low, VWAP) × trade_zone_vwap_ratio`（反彈突破）
- `(price - prev_close) / prev_close <= trade_zone_max_increase_ratio`（漲幅不超限）

### 量比計算
```cpp
rolling_sum_ratio = (rolling_sum_short / rolling_sum_long) × (long_duration / short_duration)
```
- `rolling_sum_short`：2 分鐘內盤成交量加總
- `rolling_sum_long`：10 分鐘內盤成交量加總
- 只計入 `tradeAt == 1`（內盤）的成交量

### 參數一覽（`parameter.cfg [SignalA]`）
| 參數 | 目前值 | 說明 |
|---|---|---|
| `ROLLING_LOW_DURATION` | 5 | 滑動最低價視窗（分鐘） |
| `ROLLING_SUM_SHORT_DURATION` | 2 | 短期量視窗（分鐘） |
| `ROLLING_SUM_LONG_DURATION` | 10 | 長期量視窗（分鐘） |
| `vol_contract_ratio` | 0.85 | 量縮閾值（量比 < 此值表示量縮） |
| `track_zone_vwap_ratio` | 1.003 | 追蹤區：price <= VWAP × 此值 |
| `buffer_zone_duration_min` | 4 | 緩衝區最長停留（分鐘） |
| `buffer_zone_exit_vwap_ratio` | 1.005 | 緩衝區退出：price >= VWAP × 此值 |
| `buffer_zone_day_high_ratio` | 0.986 | 緩衝區：price <= day_high × 此值 |
| `buffer_zone_vwap_entry_ratio` | 0.985 | 緩衝區：price >= VWAP × 此值 |
| `buffer_zone_start_time` | 91000000000 | 緩衝區啟用時間（09:10:00） |
| `buffer_zone_end_time` | 110000000000 | 緩衝區截止時間（11:00:00） |
| `trade_zone_duration_min` | 20 | 交易區最長停留（分鐘） |
| `trade_zone_exit_price_ratio` | 0.99 | 交易區退出：price <= trigger × 此值 |
| `trade_zone_vwap_ratio` | 1.0057 | 進場觸發：price >= max(low, vwap) × 此值 |
| `trade_zone_start_time` | 91500000000 | 交易區啟用時間（09:15:00） |
| `trade_zone_max_increase_ratio` | 0.085 | 漲幅上限（防極端股） |

---

## 3. Signal B — VWAP 上方回踩訊號（目前停用）

### 核心邏輯
股價在 VWAP 上方運行的強勢股，回踩後反彈重新突破時進場。與 Signal A 相反，Signal B 鎖定的是已在高處的股票。

### 與 Signal A 的差異
| 項目 | Signal A | Signal B |
|---|---|---|
| 追蹤區 | price <= VWAP（在均線下方） | price >= VWAP × 1.01（在均線上方） |
| 前置條件 | 無 | 09:20 後 price < VWAP × 0.995 → 永久禁止 |
| 緩衝區 | 量縮確認 | rolling_low >= prev_close × 1.06（確認強勢） |
| 交易區觸發 | 比率乘數 | tick 加減（getPriceCond） |
| 停損 | entry_vwap × 0.996 | entry_rolling_low × 0.993 |
| 重入 | 允許 | 停損過的股票永久禁止 |

### 參數（`parameter.cfg [SignalB]`，目前 `enabled=false`）
| 參數 | 目前值 | 說明 |
|---|---|---|
| `pre_condition_vwap_ratio` | 0.995 | 前置條件：price >= VWAP × 此值 |
| `track_zone_vwap_ratio` | 1.01 | 追蹤區：price >= VWAP × 此值 |
| `buffer_zone_rolling_low_increase_ratio` | 0.06 | rolling_low 需比前收高 6% |
| `trade_zone_eval_price_ratio` | 1.005 | 進場：price >= trigger × 此值 |
| `trade_zone_eval_tick_add` | 2 | 進場替代條件：price >= trigger + 2 tick |
| `trade_zone_exit_tick_sub` | 2 | 退出：price <= trigger - 2 tick |

---

## 4. 強勢個股篩選 (StrongSingle)

### 篩選流程
1. **進入監控池**：當日累計成交金額 Top-150，且月均成交金額 >= 1 億
2. **強勢條件**（任一滿足）：
   - 振幅 `(day_high - day_low) / day_low > 4%`
   - 日高漲幅 `(day_high - prev_close) / prev_close > 4%`
3. **量能條件**（任一滿足）：
   - 月均量比 `cumVol / monthAvgVol > 1.2`
   - 前日量比 `cumVol / yesterdayAvgVol > 2.0`
   - 月均成交金額 > 20 億（大型股豁免）
4. **排除條件**：
   - VWAP 保底：09:10 後 price < VWAP × 0.993 → 永久禁止（`forbidden`）
   - 極端漲幅：price 漲幅 > 8.5% → 排除
5. **族群排名門檻**（`single_group_rank_filter=true`）：
   - 若個股屬於有效族群（月均成交金額 >= 30 億），必須在該族群 R 排名 Top-1 才允許
   - 若個股不屬於任何有效族群，則豁免此檢查

### 參數（`parameter.cfg [StrongSignal]`）
| 參數 | 目前值 | 說明 |
|---|---|---|
| `monitor_pool_size` | 150 | 監控池大小 |
| `min_month_trading_val` | 100000000 | 進池門檻：月均成交金額 >= 1 億 |
| `price_amplitude_threshold` | 0.04 | 振幅門檻 4% |
| `day_high_increase_threshold` | 0.04 | 日高漲幅門檻 4% |
| `vol_increase_month_ratio` | 1.2 | 月均量比 |
| `vol_increase_yesterday_ratio` | 2.0 | 前日量比 |
| `strong_month_trading_val` | 2000000000 | 大型股豁免門檻 20 億 |
| `vwap_floor_start_time` | 91000000000 | VWAP 保底啟用時間 09:10 |
| `vwap_floor_ratio` | 0.993 | VWAP 保底比率 |
| `extreme_price_increase_limit` | 0.085 | 極端漲幅排除 8.5% |
| `single_group_rank_filter` | true | 啟用族群排名門檻 |
| `single_max_member_rank` | 1 | 族群內 R 排名需 <= 1 |

---

## 5. 強勢族群篩選 (StrongGroup)

### 族群資料
- 來源：`exec/files/group.csv`
- 格式：`族群名稱,股票代號,股票名稱`
- 一支股票可屬於多個族群

### 族群篩選條件（`isValidGroup`，全部滿足）
1. 成員月均成交金額 >= `member_min_month_trading_val`（2 億）
2. 族群月均成交金額合計 >= `group_min_month_trading_val`（30 億）
3. 族群加權/等權平均漲幅 > `group_min_avg_pct_chg`（1%）
4. 族群當日累計成交金額 / 月均 > `group_min_val_ratio`（1.2 倍）

### 族群排名與成員選取
- **全域族群排名**（`groupRank`）：依族群平均漲幅排序
- 族群需在全域 Top-`group_valid_top_n`（15）內才進入成員評估

**成員選取條件**（全部滿足）：
1. 量能條件（任一滿足）：
   - 族群月均成交金額 > 300 億 → 豁免量比檢查
   - 個股量比 >= 1.5 倍月均
   - 個股月均成交金額 > 20 億
2. `price 漲幅 > 2%` 且 `VWAP 漲幅 > 1%`
3. 非前日漲停（若啟用過濾）
4. `VWAP 漲幅 > member_vwap_pct_chg_threshold`（4%）

**選取名額**：
- Top-3 族群：最多選 `top_group_max_select`（3）名
- 其他族群：最多選 `normal_group_max_select`（2）名
- 依成員 VWAP 漲幅排序（M 排名）

### M 排名 vs R 排名

| 項目 | M 排名 (`group_member_vwapRank`) | R 排名 (`group_member_raw_vwapRank`) |
|---|---|---|
| 用途 | StrongGroup 進場選股 | StrongSingle 族群門檻檢查 |
| 篩選門檻 | 高（需通過 isValidGroup + cond1-4） | 低（僅需月均成交金額達標） |
| 更新位置 | `on_tick()` 後段（`filter_prev_day_limit_up` 之後） | `on_tick()` 前段（`filter_prev_day_limit_up` 之前） |
| 排除條件 | 前日漲停會被 filter 擋住，無法更新 M 排名 | 鎖漲停或漲幅 >= 8.5% 才排除 |
| 排序依據 | VWAP 漲幅（已篩選成員） | VWAP 漲幅（所有有效成員） |

### 參數（`parameter.cfg [StrongGroup]`）
| 參數 | 目前值 | 說明 |
|---|---|---|
| `member_min_month_trading_val` | 200000000 | 成員月均門檻 2 億 |
| `group_min_month_trading_val` | 3000000000 | 族群月均門檻 30 億 |
| `group_min_avg_pct_chg` | 0.01 | 族群最低平均漲幅 1% |
| `group_min_val_ratio` | 1.2 | 族群成交量比 1.2 倍 |
| `member_strong_vol_ratio` | 1.5 | 成員量比門檻 |
| `member_strong_trading_val` | 2000000000 | 大型股豁免 20 億 |
| `top_group_rank_threshold` | 3 | 前 3 名族群放寬選取 |
| `top_group_max_select` | 3 | 前 3 族群最多選 3 名 |
| `normal_group_max_select` | 2 | 其他族群最多選 2 名 |
| `member_vwap_pct_chg_threshold` | 0.04 | 成員 VWAP 漲幅門檻 4% |
| `group_valid_top_n` | 15 | 只考慮全域前 15 名族群 |
| `is_weighted_avg` | false | 等權平均（非成交金額加權） |
| `group_vol_ratio_exempt_threshold` | 30000000000 | 大族群量比豁免 300 億 |

---

## 6. 下單模組 (Order)

### 進場邏輯（`trigger()`）
- 進場價格使用**賣一價** `ask[0].Price`（無賣一價時用成交價）
- 固定資金 `position_cash`（1000 萬）÷ 進場價 = 持股數量
- 進場同時掛出 5 筆掛單停利（圍繞日高 ± tick 偏移）

### 停利掛單分配
將持股均分為 `take_profit_splits`（5）份，分別掛在：
```
day_high + tick_offset[i]    (tick_offset = -1, 0, 1, 2, 3)
```
例如日高 100.0，tick = 0.5：掛單價為 99.5、100.0、100.5、101.0、101.5

### 出場優先序（`on_tick()`）
1. **停損 (stopLoss)**
   - Signal A：`price <= entry_vwap × stop_loss_ratio_a`（0.996）
   - Signal B：`price <= entry_rolling_low × stop_loss_ratio_b`（0.993）
2. **時間強制出場 (timeExit)**：`matchTimeStr >= exit_time_limit`（13:25:00）
3. **停利 (takeProfit)**：成交價觸及任一掛單價位，依該價位部分出場
4. **保底出場 (bailout)**：觸發過任一停利後，若 `price <= entry_day_high × bailout_ratio`（0.985）→ 全部市價出場
5. **收盤清倉 (marketClose)**：13:25 後強制以買一價出場

### 報告產出（`generateReport()`）
- `report_trades.csv`：每筆交易詳情（進出場時間、價格、報酬率、訊號類型、族群資訊）
- `report_summary.csv`：總結統計（總交易數、勝率、平均報酬率等）
- `report_by_category.csv`：依進場原因分類的統計

### 參數（`parameter.cfg [Order]`）
| 參數 | 目前值 | 說明 |
|---|---|---|
| `position_cash` | 10000000 | 每筆交易固定資金 1000 萬 |
| `disposition_stocks_enabled` | true | 跳過處置股 |
| `filter_prev_day_limit_up` | true | 過濾前日漲停 |
| `stop_loss_ratio_a` | 0.996 | Signal A 停損比率 |
| `stop_loss_ratio_b` | 0.993 | Signal B 停損比率 |
| `bailout_ratio` | 0.985 | 停利後保底比率 |
| `entry_time_limit` | 130000000000 | 最晚進場時間 13:00 |
| `exit_time_limit` | 132500000000 | 強制出場時間 13:25 |
| `take_profit_splits` | 5 | 停利分批數 |
| `take_profit_tick_offsets` | -1,0,1,2,3 | 停利 tick 偏移 |

---

## 7. 交易日誌欄位說明

### order_log CSV 欄位
| 欄位 | 說明 |
|---|---|
| `Action` | `enter` 或 `leave` |
| `Symbol` | 股票代號 |
| `Time` | `matchTimeStr` 格式 |
| `Price` | 成交價（×10000） |
| `Cash` | 全域剩餘現金 |
| `SymbolCash` | 該股票累計損益 |
| `SignalType` | `SignalA` / `SignalB` |
| `EnterCause` | `StrongGroup` / `StrongSingle` / `Both` |
| `LeaveCause` | `stopLoss` / `takeProfit` / `bailout` / `timeExit` |
| `RemainingQty` | 剩餘持股量 |
| `GroupInfo` | 族群名稱(G族群排名/M成員排名/R原始排名) |

### GroupInfo 格式範例
- `塑膠(G2/M1/R1)`：塑膠族群，全域排名第 2，M 排名第 1，R 排名第 1
- `-`：StrongSingle 進場，無族群資訊

---

## 8. 工具類別

### GroupRank（`helper/rank.h`）
依漲幅降序排列的排名結構，使用 `map<double, string, greater<double>>`。
- `on_tick(name, gain)`：更新排名
- `getRank(name)`：取得 1-based 排名，不存在回傳 -1
- `isTopN(name, n)`：是否在前 N 名
- `erase(name)`：移除

### RollingLow / RollingSum（`helper/rolling.h`）
- `RollingLow`：滑動視窗最低值，使用單調遞增 deque
- `RollingSum`：滑動視窗加總，使用 deque

### TopKVolumeTracker（`helper/topTracker.h`）
以累計成交金額排序的 Top-K 池，使用 `set<pair<long long, string>>`。

### LinearVolumeTracker（`helper/volTracker.h`）
歷史逐筆累計量查詢，支援根據時間戳查詢某時刻的累計成交量。
