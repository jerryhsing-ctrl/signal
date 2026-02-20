# 系統核心邏輯說明 (System Logic Documentation)

本文件整合了本系統的核心邏輯，包含「篩選機制」、「訊號生成」以及「時間規則」。

> **更新日期**: 2026-01-25
> **適用版本**: C# Replay & Live Mode (By-Tick Architecture)
> **最近變更**: 強勢個股/強勢族群篩選條件優化 (昨日量增比、震幅OR漲幅、族群排名前30%)

---

## 📚 目錄 (Table of Contents)

1. [時間與執行規則](#1-時間與執行規則)
2. [篩選機制 (Screening Logic)](#2-篩選機制-screening-logic)
    - [強勢個股](#21-強勢個股-strong-stocks)
    - [強勢族群](#22-強勢族群-strong-groups)
    - [爆量族群](#23-爆量族群-burst-groups)
    - [爆量個股 (兩階段追蹤)](#24-爆量個股-intraday-burst-stocks)
3. [訊號生成機制 (Signal Generation)](#3-訊號生成機制-signal-generation)
    - [Signal A (Break & Recover)](#31-signal-a-break--recover)
    - [Signal B (Approach & Bounce)](#32-signal-b-approach--bounce)
    - [共用機制](#33-共用機制)

---

## 1. 時間與執行規則

### 三段時間管制

系統將交易日劃分為三個主要階段，不同階段有不同的行為模式。

```
09:00          09:10          09:20                     11:00          13:30
  |--------------|--------------|-------------------------|--------------|
  [  暖機期     ][  訊號產生   ][    主要交易時段        ][  僅追蹤     ]
```

| 時段 | 篩選族群/個股 | 產生 Signal A/B | 允許進場 | 說明 |
|:-----|:-------------:|:---------------:|:--------:|:----|
| **09:00 ~ 09:10** | ❌ | ❌ | ❌ | **暖機期**：累積數據，計算 VWAP 與基礎指標。 |
| **09:10 ~ 09:20** | ✅ | ✅ | ❌ | **觀察期**：開始產生篩選結果與訊號，但尚未允許進場。 |(若訊號在9:20分前觸發準備進場(第三)階段，則不進場，並移除該訊號)
| **09:20 ~ 11:00** | ✅ | ✅ | ✅ | **交易期**：全功能運作，允許產生新訊號並觸發進場。 |(同時間同一個股票只允許一次進場)
| **11:00 ~ 13:30** | ❌ | ❌ | ✅* | **收尾期**：停止產生新訊號/新篩選，僅追蹤已存在的訊號是否觸發進場或出場。 |

*註：11:00 後僅追蹤已存在之 Signal 的進場條件，不產生新 Signal。*

### 模式差異 (Replay vs Live)

| 項目 | Live 模式 | Replay 模式 |
|------|----------|-------------|
| **資料來源** | Redis 即時資料 | Quote Log (CSV) 歷史檔案 |
| **時間推進** | 真實時間 | 模擬時間 (依 Log timestamp) |
| **資料處理** | 收到即處理 (Event Driven) | 合併排序後循序處理 (Sequential) |
| **狀態重置** | 每日重啟時重置 | 每次執行 Replay 時重置 |

---

## 2. 篩選機制 (Screening Logic)

負責從全市場股票中，篩選出具備特定型態的標的。

### 2.0 預處理：月平均量能剖面 (Volume Profile Preprocessing)

> **對應服務：** `VolumeProfileService.cs`
> **快取格式：** Parquet 檔案（`avg_vol_profile_20days_min_for_YYYYMMDD.parquet`）

#### 目的

為了即時計算「量增比 (VolumeIncreaseRatio)」與「爆量比 (BurstRatio)」，系統需要歷史基準數據。預處理階段會從過去 20 個交易日的成交明細中，統計每個股票在各時間點的平均成交量。

#### 資料來源

- **輸入：** 過去 20 個交易日的 Parquet 檔案（`parquet/{market}/trades_YYYYMMDD.parquet`）
- **排除：** 試撮合資料（`FuncCode = '1'`）

#### 計算邏輯

**時間精度：** 每微秒重新運算。

**兩種量能剖面：**

1. **Cumulative（累積量）**
   - **語意：** `cumulative[HHMM00]` = 從開盤到該分鐘結束的歷史平均累積成交量
   - **範例：** `cumulative[92900]` = 9:00:00 ~ 9:29:59 的歷史平均累積量
   - **計算方式：** 先加上當前分鐘的量，再記錄（包含當前分鐘）
   - **用途：** 計算「量增比」（當前累積量 / 歷史同時段累積量）


#### 提前偵測量增的原理

**關鍵設計：** 分鐘級精度搭配「包含當前分鐘」的語意

- **查詢時機：** 實時交易時間為 `9:29:50` 時
- **查詢 timeKey：** `92950` → fallback 到 `92900`
- **歷史基準：** `cumulative[92900]` = 9:00:00 ~ 9:29:59 的歷史平均（59 秒）
- **當前累積量：** 9:00:00 ~ 9:29:50 的實際累積量（50 秒）

**量增判定：**
```
若 當前累積量（到 50 秒）>= 歷史平均（到 59 秒）× 1.2
→ 代表在該分鐘尚未結束時，就已經達到量增門檻
→ 實現「提前偵測」的效果
```

**優點：**
- 使用分鐘級歷史資料（節省儲存空間與計算成本）
- 在分鐘內任何秒數都能進行量增判斷
- 支援 By-Tick 即時篩選架構

#### 性能優化 (2026-01-26)

**VolumeProfile 資料結構優化:**
- **改進前**: 使用 `Dictionary<int, decimal>` 儲存 Rolling 與 Cumulative
  - Fallback 查詢複雜度: **O(n)** (需遍歷所有 keys)
- **改進後**: 使用 `SortedDictionary<int, decimal>`
  - Fallback 查詢複雜度: **O(log n)** (利用有序性質反向查找)
  - 預估性能提升: 在全市場 2000 檔股票、每秒 100 次 Tick 的場景下，可減少約 **60%** 的 CPU 時間

**其他優化:**
- `StockState.DayLow` 改用 `decimal?` 取代 `decimal.MaxValue` 魔術數字
- 篩選邏輯抽取獨立方法 `IsInTopPercentile`，提升單元測試性與可讀性

---

### 服務對應表

| 篩選類型 | C# 實作類別 | 狀態管理 | 生命週期 |
|:--------|:-----------|:--------:|:-------:|
| 強勢個股 | `ScreeningCalculator.FilterStrongStocks` | `ScreeningStateManager` | 20 分鐘 |
| 強勢族群 | `ScreeningCalculator.FilterStrongGroups` | `ScreeningStateManager` | 20 分鐘 |
| 爆量族群 | `ScreeningCalculator.AnalyzeBurstGroups` | `ScreeningStateManager` | 20 分鐘 |
| 爆量個股 (候選) | `BurstStockManager` | `ScreeningStateManager` | 30 分鐘* |


---

### By-Tick 觸發架構 (2026-01-23 起)

#### 觸發機制演進

| 項目 | 舊架構 | 新架構 (By-Tick) |
|------|--------|------------------|
| 篩選觸發 | 每分鐘一次 | 每 Tick (帶節流控制) |
| 狀態追蹤 | 僅 BurstStock | 僅 BurstStock |
| ExpiredAt | 僅 BurstStock | 僅 BurstStock |
| 輸出頻率 | 每分鐘 | 狀態變化時 (事件驅動) |
| 輸出精度 | HH:mm:ss | **HH:mm:ss.fff (毫秒)** |

#### 狀態追蹤機制

**核心元件**: `ScreeningStateManager` (Singleton)

**追蹤內容** (每個篩選結果):
- **DetectedAt**: 首次偵測到的時間戳 (毫秒精度)
- **ExpiresAt**: 預計失效時間 (DetectedAt + Lifetime)
- **LastRefreshedAt**: 最後一次刷新時間 (若持續符合條件)

**事件類型**:
- `Added`: 新篩選出的股票/族群
- `Refreshed`: 持續符合條件 (刷新 ExpiresAt)
- `Removed`: 不再符合條件或過期

**節流控制**:
- **MinTickIntervalMs**: 預設 100ms (避免過度頻繁執行篩選邏輯)
- **CalculationIntervalSeconds**: 預設 1 秒 (用於 ScreeningEngine，確保兩次完整計算的最小間隔)
- **診斷日誌** (`EnableDiagnosticLogging`): 預設 false，開啟後會輸出詳細的運算過程日誌用於除錯

#### 生命週期與效能配置

可在 `appsettings.json` 調整各類篩選的存活時間與性能參數:

```json
{
  "Screening": {
    "StrongStockLifetimeMinutes": 20,
    "StrongGroupLifetimeMinutes": 20,
    "BurstGroupLifetimeMinutes": 20,
    "StrongGroupTopPercentile": 0.3,
    "StrongGroupSmallGroupThreshold": 3,
    "MinTickIntervalMs": 100,
    "CalculationIntervalSeconds": 1,
    "EnableDiagnosticLogging": false
  }
}
```

---

### 2.1 強勢個股 (Strong Stocks)

篩選出市場中資金集中且型態強勢的領頭羊。

#### 流程與條件
1.  **建立監控池**:
    *   取 當日成交值 (Turnover)` 前 **200** 名 (可配置 `StrongStockTopN`)。

2.  **核心條件 (AND)**:
    *   **A. 價格型態** (可配置開關 `UseMaxRangeFilter`):
        * 曾經最大震幅 `(DayHigh - DayLow) / DayLow` >= **4.0%**  **OR** `(DayHigh - 昨收) / 昨收`漲幅 >= **4.0%**     
        
    *   **B. 量能支撐** (OR 邏輯):
        *   月均量增比 `VolumeIncreaseRatio` >= **1.2** (當前累積量 / 月均同時段累積量)
        *   **OR** 昨日量增比 `VolumeIncreaseRatioPast1` >= **2.0** (當前累積量 / 昨日同時段累積量)
        *   **OR** 月均成交值 `Turnover` >= **2 billion**(近20天的收盤總成交金額取平均)

    *   **C. VWAP Floor**: 價格於9:20分後，從未跌破 `VWAP × (0.995-台指期跌幅%*0.005)` (一旦跌破即永久失去當日資格)。//////////////////////////////////
        * 即使價格回升到 VWAP 之上，也無法再進入強勢個股
        * 若台指期為正值，則維持0.995，不另行往上調整。
      
    *   **D. 極端值過濾**: 當前漲幅 `ChangePct` <= **8.5%** (排除已漲停鎖死)。

#### 結果排序
*   依現價漲幅 `ChangePct` 降冪排序。



---

### 2.2 強勢族群 (Strong Groups)

篩選出整體資金流向明顯、齊漲或齊量的產業族群。

--------------- 以下還沒實作 要補 ---------------
### 族群平均計算濾網

> [!IMPORTANT]
> **計算族群平均時，會先過濾不符資格的小型股**

計算 `AvgChangePct` (平均漲幅)時，只納入符合以下條件的股票：

1. **成交金額門檻**: 月平均成交金額 (Price × VolumeAccumulated) >= **0.1 billion**
   → 對應變數：`GroupAvgMinTurnover`



#### 族群入選條件
1.  **流動性**: 族群總月均額 >= **30 億**。 對應變數：`StrongGroupLiquidity`。
2.  **強度指標**:
    *   當前平均漲幅 `AvgChangePct` >= **1.5%+台指期漲幅%***。(台指漲1%，平均漲幅就需要2.5%，反之台指跌1%，平均漲幅只需要0.5%)\
    *   當前平均量比達標: `AvgVolumeIncreaseRatio` >= **1.0** → 對應變數：`StrongGroupVolMult`。
        (平均量比的計算，由族群成交值前五名的月均量增比取平均後得出)
3.  **數量限制**: 按族群平均漲幅排序，最多同時顯示前 **20** 個族群 (可配置 `MaxGroupCount`)。



#### 族群內個股顯示條件
入選族群後，列表只顯示符合以下條件的個股：
1.  **條件一**: 個股量增比 >= **1.5** **AND** 個股當前漲幅 >= `族群平均漲幅 - 0.5%` **OR** 個股當前漲幅 >= `4%`
    * 即使個股漲幅低於族群平均，只要量增比達標且漲幅超過4%，也會被納入。
    * 納入的個股排序為漲幅由高至低
2.  **條件二**: 個股月平均成交值 >= 0.3 billion 

> **目的**: 除了漲幅接近族群平均的股票外，也納入漲幅相對較高的股票（前N%），避免遺漏族群內的領漲股。

---

### 2.3 爆量族群 (Burst Groups)(爆量族群僅供觀察，不進入ABC訊號的濾網)

篩選出盤中突然出現異常大量交易的族群。

#### 族群入選條件
1.  **流動性**: 族群總月均額 >= **20 億**。
2.  **爆量強度**:
    *   **族群平均爆量比** `AvgBurstRatio` > **1.25**。

--------------- 以下還沒實作 要補 ---------------
#### 計算邏輯

> [!IMPORTANT]
> **計算族群平均爆量比時，會先過濾不符資格的小型股**

```
對每個族群:
  1. 篩選成交金額 >= GroupAvgMinTurnover 的股票
  2. 依爆量比 (BurstRatio) 降冪排序，取前 GroupAvgTopN 名
  3. 計算平均爆量比 = Sum(過濾後股票的 BurstRatio) / 股票數
  4. 若 AvgBurstRatio > 門檻 且 TotalLiquidity >= 20億
     → 列入爆量族群
```
--------------- 以上還沒實作 要補 ---------------


#### 爆量比 (BurstRatio) 計算
> [!IMPORTANT]
> **爆量比 (BurstRatio)** 的比較基準是 **盤中短期量能**，與過去 20 天歷史平均無關。
*   **公式**: `近 5 分鐘均量 / 近 30 分鐘均量`。
*   **計算邏輯**: 同樣先排除成交金額小的股票，取爆量比前 N 名計算平均。
*   **處置股排除**: 處置股票 (交易間隔 >= 4分鐘) 不納入計算。若超過 → 永久標記 `_isDispositionStock = true`

--------------- 以下參數設定還沒實作 要補 ---------------
- 預設值：N = 5 分鐘、M = 30 分鐘 → 可透過 `BurstShortWindowMinutes` / `BurstLongWindowMinutes` 配置
--------------- 以上參數設定還沒實作 要補 ---------------

#### 狀態追蹤 (2026-01-23 起)
*   **生命週期**: 預設 **20 分鐘** (可配置)。
*   **持續條件**: 族群爆量比持續 > 1.25 時，刷新生命週期。
*   **爆量結束**: 若爆量比回落至門檻以下，產生 `Removed` 事件。

---
--------------- 以下邏輯有待確認 理論上與現有代碼一致 ---------------
### 2.4 爆量個股 (Intraday Burst Stocks)

篩選出短時間內成交量急遽放大的個股，採用**兩階段檢查機制**。
僅在經過9:00開盤30分後進行爆量個股分析

### 狀態定義

爆量個股使用統一的追蹤模型 `BurstStockRecord`，包含以下狀態：

| 狀態 (Phase)  | 說明                              | 過期時間                                          |
| :------------ | :-------------------------------- | :------------------------------------------------ |
| `Candidate` | 第一階段通過，等待 VWAP 驗證      | DetectedAt + 30 分鐘 (分鐘參數可由appsetting調整) |
| `Confirmed` | 第二階段通過，可進入 A/B 訊號追蹤 | PromotedAt + 30 分鐘 (分鐘參數可由appsetting調整) |
| `Expired`   | 已過期，從顯示清單移除            | -                                                 |

> [!NOTE]
> **ExpiresAt 欄位共用**：無論 Candidate 或 Confirmed 階段，都使用同一個 `ExpiresAt` 欄位記錄過期時間。
>
> - Candidate 階段：`ExpiresAt = DetectedAt + 30 分鐘`
> - Confirmed 階段：`ExpiresAt = PromotedAt + 30 分鐘`（升級時覆蓋）(改成30分鐘)

---

### 第零階段：監控池篩選

為了避免冷門股因成交量基數過低而產生異常爆量，我們僅針對當日成交值前 N 名的股票進行監控。



1. 取成交值 Top N：依據 `成交值 = Price × VolumeAccumulated` 降冪排序，取前 **200** 名

* **對應配置**: `BurstStockTopN` (預設 200)
  → **appsettings 區段**: `Screening` → `"BurstStockTopN": 200`

---

### 處置股偵測與排除  (Global排除機制)

> [!WARNING]
> **處置股（每 5 或 20 分鐘撮合一次）會導致爆量比異常，需永久排除。**

**偵測機制**：

- 每次收到 Trade 時，檢查距離上次成交是否 ≥ 4 分鐘
- 若超過 → 永久標記 `IsDispositionStock = true`
- 標記後 `CalculateBurstRatio()` 恆返回 0，不納入任何爆量判定

對應欄位：`StockState.IsDispositionStock`

---

### 兩階段判定邏輯（By Tick 檢查）

> [!IMPORTANT]
> **By Tick 檢查機制**：
> 每收到一筆交易資料（Trade）就執行一次完整的階段檢查流程，不是定時檢查。

#### 第一階段：基礎檢查 (量能 + 價格波動)

同時滿足以下兩個條件才通過基礎檢查：

1. **量能條件**: `爆量比 (BurstRatio)` > **1.5**

   * **計算公式**:
     * `短均量` = 近 5 分鐘成交量 ÷ 5 (分鐘參數可由appsetting調整)
     * `長均量` = 近 30 分鐘成交量 ÷ 30 (分鐘參數可由appsetting調整)
     * `爆量比 (BurstRatio)` = 短均量 ÷ 長均量
   * **對應配置**: `BurstMultiplier` (預設 1.5)
     → **appsettings 區段**: `VWAPSignal` → `"BurstMultiplier": 1.5`
2. **價格波動條件**: `當前價格 / 近5分鐘低點` > **1.008** (0.8%)

   * **目的**: 確保股票有實際波動性，排除只有量能但價格沒動的情況
   * **對應配置**: `BurstPriceVolatilityThreshold` (預設 1.008)
     → **appsettings 區段**: `Screening` → `"BurstPriceVolatilityThreshold": 1.008`
--------------- 以上邏輯有待確認 理論上與現有代碼一致 ---------------

#### 第二階段：VWAP 驗證（觀察期管理）

對於通過第一階段檢查的股票，每收到一筆 Trade 時：

* 1. 檢查是否過期：

   
  若 當前時間 > ExpiresAt → 移除追蹤（Phase = Expired）
* 2. VWAP 驗證（未過期時）：

   
  若 Price >= VWAP → 升級為 Confirmed
  Phase = Confirmed
PromotedAt = 當前時間（升級時間）
ExpiresAt = PromotedAt + 30 分鐘（覆蓋原 Candidate 過期時間）
若 Price < VWAP → 保持 Candidate，繼續觀察

---

#### 確認股（Confirmed）的生命週期

確認股過期檢查：

   
每收到 Trade 時，檢查 當前時間 > ExpiresAt
若過期 → 移除追蹤（Phase = Expired），從顯示清單移除
確認股不再受第一階段影響：

   
即使該股票再次觸發第一階段條件，也不會刷新 Confirmed 的過期時間
Confirmed 狀態的過期時間固定為 PromotedAt + 30 分鐘
---

### 2.5 篩選結果輸出格式

**輸出欄位** (StockScreeningRecord):
- `EventType`: 事件類型 (Added / Refreshed / Removed)
- `ScreeningCategory`: 篩選類別 (StrongStock / StrongGroup / BurstGroup / BurstStockCandidate)
- `DetectedAt`: 首次偵測時間 (HH:mm:ss.fff)
- `ExpiresAt`: 預計失效時間 (HH:mm:ss.fff)
- `LastRefreshedAt`: 最後刷新時間 (HH:mm:ss.fff，可為空)
- *(其他原有快照數值欄位...)*

**輸出時機**:
- 僅在狀態變化時輸出 (Added / Refreshed / Removed 事件)
- 若無狀態變化，該 Tick 不產生輸出

**輸出格式**:
- Parquet 檔案 (含 Schema 更新)
- XLSX 檔案 (含新欄位)

---
---

## 3. 訊號生成機制 (Signal Generation)

> [!IMPORTANT]
> **架構異動 (2026-01-22)**
> 訊號生成邏輯 (Signal A/B) 已移至外部 **C++ 運算核心** 執行。
> C# 端僅負責將篩選出的「候選股票」送往 C++，並接收 C++ 回傳的訊號結果。

### 3.1 運作流程 (C# <---> C++)

1.  **C# 篩選 (Screening)**:
    *   計算 **強勢個股**、**強勢族群**、**爆量族群**、**爆量個股**。
    *   產生一份「候選監控清單 (Candidate List)」。

2.  **資料傳送 (Data Handoff)**:
    *   C# 將候選清單與即時 Tick 資料傳送給 C++ 核心。

3.  **C++ 訊號運算 (Signal Calculation)**:
    *   C++ 負責執行高頻運算，包含：
        *   **Signal A (Break & Recover)**: 跌破 VWAP 後快速反彈。
        *   **Signal B (Approach & Bounce)**: 接近 VWAP 後強勢反彈。
        *   **微結構判定**: 包含 Tick-level 的量縮 (Volume Contraction)、掛單失衡 (Order Imbalance) 等高頻指標。

4.  **訊號回傳 (Feedback)**:
    *   當 C++ 偵測到訊號觸發進場條件時，回傳訊號至 C#。
    *   C# 接收後進行後續處理 (如：顯示前端、紀錄 Log、發送通知)。

---

### 3.2 訊號類型 (Reference Only)

詳細演算法請參閱 C++ 專案文件，此處僅列出訊號定義供 C# 開發者參考。

| 訊號類型 | 描述 | 適合情境 |
|:--------|:-----|:--------|
| **Signal A** (Break & Recover) | 價格短暫跌破 VWAP 後，在短時間內伴隨大量買盤拉回。 | 強勢股洗盤、誘空後軋空。 |
| **Signal B** (Approach & Bounce) | 價格在 VWAP 上方運行，回測 VWAP 不破並出現支撐。 | 超強勢股沿均線推升。 |

---

### 3.3 共用機制

*   **Rolling Low**: 由 C++ 維護。
*   **量縮 (Volume Contraction)**: 由 C++ 維護。
*   **Tick Size**: C# 與 C++ 共用相同的 Tick Size 表。

