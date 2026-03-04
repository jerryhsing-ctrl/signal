# 台股盤中動量回測系統

## 專案概述
台灣股票市場盤中動量回測系統。透過歷史逐筆行情資料重播，使用三階段訊號模型（追蹤區 → 緩衝區 → 交易區）配合強勢股/強勢族群篩選，進行日內動量交易策略回測。

## 建置與執行

```bash
make          # 編譯，產出 build/HWStratSv，自動複製到 exec/sv/signal
make run      # 編譯並執行（在 exec/ 目錄下跑 ./sv/signal）
make asan     # AddressSanitizer 模式（-fsanitize=address -g -O0）
make debug    # Debug 模式（-g -O0）+ 啟動 gdb
make clean    # 清除 build/ 目錄
make kill     # pkill -f signal
```

- 編譯器：`g++ -std=c++20 -Wall -pthread`
- 執行方式：`cd exec && ./sv/signal [YYYYMMDD]`，日期預設為 main.cpp 中硬編碼的值
- 執行緒策略：主要策略迴圈釘在 CPU core 20，SCHED_FIFO 優先級 99

## 目錄結構

```
signal/
├── main.cpp                # 程式入口：載資料 → 啟動策略執行緒
├── Makefile                # 建置系統
├── chooseStock.md          # 選股邏輯原始設計文件
│
├── Order/                  # 下單模組
│   ├── Order.h / .cpp      # 進場、停損、停利、報告產生
│
├── QuoteSv/                # 行情服務
│   ├── QuoteSv.h / .cpp    # 歷史逐筆資料讀取、月均量計算
│   ├── Format1.h / .cpp    # 前收盤價、漲跌停資訊
│   ├── Format6.h / .cpp    # 逐筆行情資料格式（主要 tick 資料結構）
│   └── UDPSession.h / .cpp # UDP 接收（目前未啟用）
│
├── StrategySv/             # 策略服務
│   ├── StrategySv.h / .cpp # 策略主迴圈：每筆 tick 的評估流程
│   ├── signalA.h / .cpp    # Signal A：碰均線後量縮反彈進場
│   ├── signalB.h / .cpp    # Signal B：VWAP 上方回踩進場（目前停用）
│   ├── strongGroup.h / .cpp# 強勢族群篩選 + M/R 排名
│   ├── strongSingle.h      # 強勢個股篩選
│   └── indexCalc.h         # IndexData 計算（VWAP、日高低）
│
├── helper/                 # 工具類別（多為 header-only）
│   ├── rank.h              # GroupRank：依漲幅排序的排名結構
│   ├── rolling.h           # RollingLow（滑動最低值）/ RollingSum（滑動加總）
│   ├── tick.h              # 股票分類與跳動點位（GetTick, getPriceCond）
│   ├── topTracker.h        # TopKVolumeTracker：依累計成交值排序的 Top-K 池
│   ├── volTracker.h        # LinearVolumeTracker：歷史逐筆累計量查詢
│   ├── type.h              # format6Type、MatchType enum、queueType
│   ├── core.h              # 執行緒綁核、即時排程
│   ├── SPSC.h              # Lock-free 單生產者單消費者佇列
│   ├── IniReader.h / .cpp  # INI 設定檔解析器
│   └── ...                 # 其他工具
│
├── exec/                   # 執行環境
│   ├── cfg/
│   │   ├── HWStratSv.cfg   # 網路 / 行情源設定（目前全部關閉，檔案重播模式）
│   │   └── parameter.cfg   # 所有策略參數（Signal A/B、StrongSingle/Group、Order）
│   ├── data/               # 歷史逐筆行情檔（OTCQuote.YYYYMMDD）
│   ├── files/              # Symbols_YYYYMMDD.csv（前日漲停檢查用）、group.csv
│   ├── sv/signal           # 編譯後的執行檔
│   └── log/                # 每次執行的輸出日誌
│       └── YYYYMMDD_HHMM/
│           ├── order_log_YYYYMMDD.csv          # 全部交易紀錄
│           ├── order_log_YYYYMMDD_XXXX.csv     # 個股交易紀錄
│           ├── report_trades.csv               # 每筆交易報告
│           ├── report_summary.csv              # 總結報告
│           └── report_by_category.csv          # 分類報告
│
└── build/                  # 編譯產物（.o / .d 檔案）
```

## 程式碼慣例

- **價格單位**：所有價格以 `long long` 儲存，乘以 10000（例如 50.00 元 → 500000）
- **時間格式**：
  - `matchTime_us`：微秒（用於 rolling window 計算）
  - `matchTimeStr`：整數格式如 `91500000000` 代表 09:15:00.000000
- **設定檔**：所有策略參數集中在 `exec/cfg/parameter.cfg`，使用 INI 格式，分為 `[SignalA]`、`[SignalB]`、`[StrongSignal]`、`[StrongGroup]`、`[Order]` 等 section
- **日誌輸出**：交易日誌寫入 `exec/log/YYYYMMDD_HHMM/` 資料夾，包含主日誌和個股日誌

## 重要注意事項

- 修改參數後只需重新執行即可，不需重新編譯
- 修改 `.cpp` / `.h` 後需 `make` 重新編譯
- 回測日期在 `main.cpp` 中硬編碼，需要手動修改
- 系統目前為檔案重播（回測）模式，所有 UDP 行情源已關閉
- `group.csv` 定義族群成員，格式為：`族群名稱,股票代號,股票名稱`
