#!/bin/bash

# ================= 設定區 =================
OUTPUT_FILE="full_market_replay.pcap"
PASSWORD="holdwin@3214"  # 您的 sudo 密碼
USER="root"           # 您的使用者名稱 (用來修正檔案權限)

# 監聽網卡設定 (使用 any 雖方便，但建議明確指ㄐ定介面以減少雜訊，若不確定流量走哪張卡則維持 any)
INTERFACE="any"

# ================= 過濾目標配置 =================
# 錄製模式設定
# "ALL"      : 錄製 TCP 和 UDP (預設)
# "UDP_ONLY" : 只錄製 UDP
RECORD_MODE="ALL"

# TCP 目標
TCP_TARGET_IP="10.101.1.102"
TCP_PORT="9003"

# UDP 來源 (PCAP 中要過濾的)
UDP_SOURCE_IP="224.0.100.100"
UDP_SOURCE_PORT="10000"
# UDP_SOURCE_IP="224.0.100.100"
# UDP_SOURCE_PORT="10000"

# UDP 回放目標 (發送到 Server)
UDP_REPLAY_IP="239.255.0.1"
UDP_REPLAY_PORT="10000"

# 本機網卡 IP
# MCAST_INTERFACE_IP="10.71.17.115"
MCAST_INTERFACE_IP="192.168.204.141"

# ================= 自動生成過濾規則 =================
if [ "$RECORD_MODE" == "UDP_ONLY" ]; then
    FILTER="udp and dst host ${UDP_SOURCE_IP} and dst port ${UDP_SOURCE_PORT}"
else
    # 預設為 ALL (TCP + UDP)
    FILTER="(tcp and dst host ${TCP_TARGET_IP} and dst port ${TCP_PORT}) or (udp and dst host ${UDP_SOURCE_IP} and dst port ${UDP_SOURCE_PORT})"
fi

# ================= 函數定義 =================
function toggle_mode() {
    if [ "$RECORD_MODE" == "ALL" ]; then
        RECORD_MODE="UDP_ONLY"
    else
        RECORD_MODE="ALL"
    fi
    
    # 更新過濾規則
    if [ "$RECORD_MODE" == "UDP_ONLY" ]; then
        FILTER="udp and dst host ${UDP_SOURCE_IP} and dst port ${UDP_SOURCE_PORT}"
    else
        FILTER="(tcp and dst host ${TCP_TARGET_IP} and dst port ${TCP_PORT}) or (udp and dst host ${UDP_SOURCE_IP} and dst port ${UDP_SOURCE_PORT})"
    fi
}

function start_recording() {
    echo "========================================================"
    echo "  🎥 正在開始全景錄製 (行情 + 下單)"
    echo "  ----------------------------------------------------"
    echo "  介面: $INTERFACE"
    echo "  輸出: $OUTPUT_FILE"
    echo "  過濾: $FILTER"
    echo "========================================================"
    echo "  🔴 錄製中... 請開始您的測試操作 (按 Ctrl+C 停止)..."

    # 設置 trap 來正確處理 Ctrl+C
    trap 'echo ""; echo "正在停止錄製..."; sudo pkill -SIGTERM tcpdump; sleep 2; return' INT

    # 在背景執行 tcpdump
    # -nn: 不解析域名 (加速)
    # -s 0: 抓取完整封包內容 (關鍵，否則大封包會被截斷)
    echo "  執行指令: sudo tcpdump -i \"$INTERFACE\" -nn -s 0 -w \"$OUTPUT_FILE\" \"$FILTER\""
    echo "${PASSWORD}" | sudo -S tcpdump -i "$INTERFACE" -nn -s 0 -w "${OUTPUT_FILE}" "${FILTER}" &
    TCPDUMP_PID=$!

    # 等待 tcpdump 結束
    wait $TCPDUMP_PID 2>/dev/null

    echo ""
    echo "========================================================"
    echo "  🛑 錄製結束"

    # 修正檔案權限 (因為是用 root 錄的，需改回 devuser 讓 Python 讀取)
    if [ -f "$OUTPUT_FILE" ]; then
        echo "${PASSWORD}" | sudo -S chown ${USER}:${USER} "${OUTPUT_FILE}"
        echo "  ✅ 權限已修正為 $USER"
        
        # 簡單統計
        COUNT=$(tcpdump -r "$OUTPUT_FILE" 2>/dev/null | wc -l)
        echo "  📊 總共捕獲封包數: $COUNT"
    else
        echo "  ❌ 錯誤：找不到輸出檔案"
    fi
    echo "========================================================"
}

function compile_and_replay() {
    echo "========================================================"
    echo "  🔧 編譯 replay.cpp..."
    echo "========================================================"
    
    if g++ -std=c++11 -o replay replay.cpp -lpcap; then
        echo "  ✅ 編譯成功！"
        echo ""
        echo "========================================================"
        echo "  🚀 開始回放..."
        echo "  ----------------------------------------------------"
        echo "  TCP 目標: ${TCP_TARGET_IP}:${TCP_PORT}"
        echo "  UDP 來源: ${UDP_SOURCE_IP}:${UDP_SOURCE_PORT}"
        echo "  UDP 回放: ${UDP_REPLAY_IP}:${UDP_REPLAY_PORT}"
        echo "========================================================"
        # 通過命令行參數傳遞配置
        ./replay "${OUTPUT_FILE}" \
                 "${TCP_TARGET_IP}" "${TCP_PORT}" \
                 "${UDP_SOURCE_IP}" "${UDP_SOURCE_PORT}" \
                 "${UDP_REPLAY_IP}" "${UDP_REPLAY_PORT}" \
                 "${MCAST_INTERFACE_IP}" \
                 "true"
    else
        echo "  ❌ 編譯失敗！"
        read -p "按 Enter 繼續..."
    fi
}

# ================= 主選單 =================
while true; do
    clear
    echo "========================================================"
    echo "            📼 PCAP 錄製與回放工具"
    echo "========================================================"
    echo ""
    echo "  1️⃣  開始錄製 (tcpdump)"
    echo "  2️⃣  編譯並執行回放 (replay.cpp)"
    echo "  3️⃣  切換錄製模式 (目前: $RECORD_MODE)"
    echo "  4️⃣  退出"
    echo ""
    echo "========================================================"
    echo "  輸出檔案: $OUTPUT_FILE"
    echo "  過濾規則: $FILTER"
    echo "========================================================"
    echo ""
    read -p "請選擇操作 [1-4]: " choice

    case $choice in
        1)
            start_recording
            read -p "按 Enter 返回選單..."
            ;;
        2)
            compile_and_replay
            read -p "按 Enter 返回選單..."
            ;;
        3)
            toggle_mode
            ;;
        4)
            echo "👋 再見！"
            exit 0
            ;;
        *)
            echo "❌ 無效選項，請輸入 1-4"
            sleep 1
            ;;
    esac
done