# PCAP 錄製與回放工具

## 設定

在使用前，請先修改 `record.sh` 中的以下參數：

```bash
# ================= 設定區 =================
PASSWORD="your_sudo_password"    # 您的 sudo 密碼
USER="devuser"                   # 您的使用者名稱

# 監聽網卡
INTERFACE="any"                  # 建議改為具體網卡名稱（如 eth0）

# TCP 目標
TCP_TARGET_IP="10.101.1.102"    # TCP 伺服器 IP
TCP_PORT="9005"                  # TCP 伺服器 Port

# UDP 來源（PCAP 中要錄製的）
UDP_SOURCE_IP="224.0.100.100"   # UDP 行情來源 IP
UDP_SOURCE_PORT="10000"          # UDP 行情來源 Port

# UDP 回放目標（發送到 Server）
UDP_REPLAY_IP="239.255.0.1"     # UDP 回放目標 IP
UDP_REPLAY_PORT="10000"          # UDP 回放目標 Port

# 本機網卡 IP
MCAST_INTERFACE_IP="10.71.17.115"  # 用於 Multicast 的本機 IP
```

## 使用方法

### 啟動工具
```bash
./record.sh
```

### 功能選單

1. **開始錄製**
   - 使用 tcpdump 錄製 TCP 下單和 UDP 行情
   - 按 `Ctrl+C` 停止錄製
   - 輸出檔案：`full_market_replay.pcap`

2. **編譯並回放**
   - 自動編譯 `replay.cpp`
   - 按照真實時間回放錄製的資料
   - TCP 和 UDP 會同步發送

3. **退出**
   - 結束程式

## 注意事項

- 錄製需要 sudo 權限
- 確保設定的 IP 和 Port 正確
- 回放前請確認目標 Server 已啟動
