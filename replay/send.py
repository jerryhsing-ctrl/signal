#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import socket
import time
import sys
import struct
import os
from scapy.all import *

# ================= 設定區 =================
PCAP_FILE = "full_market_replay.pcap"

# Server 設定
TCP_TARGET_IP = "10.101.1.102"
TCP_PORT      = 9005

UDP_TARGET_IP = "239.255.0.1"
UDP_PORT      = 10000

# 本機發送 Multicast 使用的網卡 IP (必須是 Server 能收到 multicast 的網段)
# 根據您的 ip a，這是 eno2 的 IP
MCAST_INTERFACE_IP = '10.71.17.115'

# True = 模擬原本速度 (推薦)
# False = 全速發送
REAL_TIME_REPLAY = True
# =========================================

def main():
    if not os.path.exists(PCAP_FILE):
        print(f"❌ 錯誤: 找不到檔案 {PCAP_FILE}")
        sys.exit(1)

    file_size_mb = os.path.getsize(PCAP_FILE) / (1024 * 1024)
    print(f"[*] 正在讀取 PCAP: {PCAP_FILE} ({file_size_mb:.1f} MB)")
    
    # ---------------------------
    # 1. 準備 Socket
    # ---------------------------
    
    # TCP Socket
    tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_connected = False
    
    # UDP Socket
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
    udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    # [關鍵] 設定 Multicast 出去的介面
    try:
        udp_sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, 
                            socket.inet_aton(MCAST_INTERFACE_IP))
    except OSError as e:
        print(f"❌ 設定 Multicast 介面 ({MCAST_INTERFACE_IP}) 失敗: {e}")
        print("   請確認該 IP 是否存在於本機 'ip a' 列表中。")
        sys.exit(1)

    # 開啟 Loopback (讓本機 Server 也能收到)
    udp_sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
    
    packets_to_process = []

    # ---------------------------
    # 2. 讀取並過濾封包
    # ---------------------------
    try:
        # 使用 PcapReader 逐個讀取以節省記憶體
        print("[*] 開始掃描封包 (使用 PcapReader)...")
        with PcapReader(PCAP_FILE) as pcap_reader:
            for pkt in pcap_reader:
                if not pkt.haslayer(IP):
                    continue
                
                ip_layer = pkt[IP]
                
                # --- 篩選 TCP (Client -> Server) ---
                if pkt.haslayer(TCP):
                    # 檢查是否為發往 Server 的資料
                    if ip_layer.dst == TCP_TARGET_IP and pkt[TCP].dport == TCP_PORT:
                        payload = bytes(pkt[TCP].payload)
                        if len(payload) > 0:
                            packets_to_process.append(('TCP', pkt.time, payload))

                # --- 篩選 UDP (Multicast Data) ---
                elif pkt.haslayer(UDP):
                    if ip_layer.dst == UDP_TARGET_IP and pkt[UDP].dport == UDP_PORT:
                        payload = bytes(pkt[UDP].payload)
                        if len(payload) > 0:
                            packets_to_process.append(('UDP', pkt.time, payload))

        if not packets_to_process:
            print("❌ 錯誤：PCAP 中找不到任何符合條件的 Payload。")
            sys.exit(1)

        print(f"[*] 讀取完成，共 {len(packets_to_process)} 個封包需回放。")

        # ---------------------------
        # 3. 建立 TCP 連線
        # ---------------------------
        # 檢查是否有 TCP 封包需發送，若有才連線
        has_tcp = any(p[0] == 'TCP' for p in packets_to_process)
        
        if has_tcp:
            print(f"[*] 嘗試連線 TCP Server {TCP_TARGET_IP}:{TCP_PORT} ...")
            try:
                tcp_sock.connect((TCP_TARGET_IP, TCP_PORT))
                tcp_connected = True
                print("✅ TCP 連線建立成功！")
            except ConnectionRefusedError:
                print("❌ 連線失敗：Server 拒絕連線。請確認 Server 已啟動。")
                sys.exit(1)
            except Exception as e:
                print(f"❌ 連線錯誤: {e}")
                sys.exit(1)
        else:
            print("[*] 無 TCP 資料，僅回放 UDP。")

        # ---------------------------
        # 4. 開始回放迴圈
        # ---------------------------
        print("\n🚀 開始回放資料...")
        
        # 基準時間點
        start_wall_time = time.time()
        start_pcap_time = packets_to_process[0][1]
        
        tcp_sent = 0
        udp_sent = 0

        for proto, pkt_time, payload in packets_to_process:
            # === 時間同步 ===
            if REAL_TIME_REPLAY:
                # 計算該封包在錄製時，距離第一個封包過了多久
                offset = pkt_time - start_pcap_time
                # 計算現在實際過了多久
                current_elapsed = time.time() - start_wall_time
                
                wait_time = offset - current_elapsed
                if wait_time > 0:
                    time.sleep(wait_time)

            # === 發送邏輯 ===
            if proto == 'TCP' and tcp_connected:
                payload = bytes(payload) # 確保是 bytes
                
                # 防呆：至少要有 Header (4 bytes Len + 1 byte Type = 5 bytes)
                if len(payload) < 5:
                    continue

                try:
                    # 1. 解析 Header 裡的長度 (Big Endian uint32)
                    # 這是 Protocol 規定的 Body 長度
                    body_len = struct.unpack('>I', payload[0:4])[0]
                    
                    # 2. 計算這個封包「應該」要有的總長度
                    # Total = 4 (Len欄位) + 1 (Type欄位) + body_len (Body長度)
                    expected_total_len = 4 + 1 + body_len
                    
                    # 3. 檢查實際 Payload 是否過長 (這就是問題所在!)
                    if len(payload) > expected_total_len:
                        # print(f"[TCP] ⚠️ 發現 Padding! 實際: {len(payload)}, 應為: {expected_total_len}。自動修剪。")
                        # 【關鍵修正】：只發送協議規定的長度，切掉後面的 Padding
                        real_payload = payload[:expected_total_len]
                        tcp_sock.sendall(real_payload)
                        tcp_sent += 1
                        
                        # 顯示 Log
                        pkt_type = payload[4]
                        # print(f"[TCP #{tcp_sent}] Sent {len(real_payload)} bytes (Trimmed from {len(payload)})")

                    elif len(payload) == expected_total_len:
                        # 長度完全正確，直接發送
                        tcp_sock.sendall(payload)
                        tcp_sent += 1
                        # pkt_type = payload[4]
                        # print(f"[TCP #{tcp_sent}] Sent {len(payload)} bytes")
                        
                    else:
                        # len(payload) < expected_total_len
                        # 這代表封包被截斷了 (錄製不完整)，發送了也沒用，Server 會卡住等待
                        print(f"[TCP] ❌ 封包殘缺，跳過 (實際: {len(payload)}, 預期: {expected_total_len})")
                        continue

                except struct.error:
                    print(f"[TCP] 解析 Header 失敗，跳過")
                except BrokenPipeError:
                    print("\n❌ Server 斷線！停止 TCP 發送。")
                    tcp_connected = False
            
            elif proto == 'UDP':
                udp_sock.sendto(payload, (UDP_TARGET_IP, UDP_PORT))
                udp_sent += 1
                if udp_sent % 100 == 0:
                    sys.stdout.write(f"\r[UDP] 已廣播 {udp_sent} 筆行情...")
                    sys.stdout.flush()

    except KeyboardInterrupt:
        print("\n\n⏹️ 使用者中斷回放")
    except Exception as e:
        print(f"\n❌ 未預期的錯誤: {e}")
    finally:
        if tcp_connected:
            tcp_sock.close()
        udp_sock.close()
        print(f"\n\n[*] 回放結束。統計: TCP={tcp_sent}, UDP={udp_sent}")

if __name__ == "__main__":
    main()