#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <csignal>
#include <pcap.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ether.h>
#include <unistd.h>

// 定義 DLT_LINUX_SLL2 (如果系統頭文件沒有)
#ifndef DLT_LINUX_SLL2
#define DLT_LINUX_SLL2 276
#endif

// ================= 預設設定 =================
const char* DEFAULT_PCAP_FILE = "full_market_replay.pcap";
const char* DEFAULT_TCP_TARGET_IP = "10.101.1.102";
const int DEFAULT_TCP_PORT = 9005;
const char* DEFAULT_UDP_SOURCE_IP = "224.0.100.100";
const int DEFAULT_UDP_SOURCE_PORT = 10000;
const char* DEFAULT_UDP_REPLAY_IP = "239.255.0.1";
const int DEFAULT_UDP_REPLAY_PORT = 10000;
const char* DEFAULT_MCAST_INTERFACE_IP = "10.71.17.115";
const bool DEFAULT_REAL_TIME_REPLAY = true;

// 全速模式下的流控設定 (避免 Server 處理不過來)
// Python 版本因為 Scapy 解析慢，平均每包有 ~100ms 延迟，所以不會讓 Server 阻塞
// C++ 版本太快，需要手動加入流控
const int FAST_MODE_BATCH_SIZE = 5;    // 每發送 N 個包後休息
const int FAST_MODE_SLEEP_US = 10000;  //
// =========================================

int tcp_sockfd = -1;
int udp_sockfd = -1;
bool tcp_connected = false;

// 訊號處理：捕捉 Ctrl+C 優雅退出
void signal_handler(int signum) {
    std::cout << "\n[!] 停止回放..." << std::endl;
    if (tcp_sockfd != -1) close(tcp_sockfd);
    if (udp_sockfd != -1) close(udp_sockfd);
    exit(signum);
}

// 取得當前微秒時間戳 (用於時間同步)
uint64_t get_current_micros() {
    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
}

// 類似 Python sendall() 的函數：確保所有數據都發送完畢
bool send_all(int sockfd, const void* data, size_t len) {
    const char* ptr = (const char*)data;
    size_t remaining = len;
    
    while (remaining > 0) {
        ssize_t sent = send(sockfd, ptr, remaining, 0);
        if (sent <= 0) {
            // 發送失敗或連線中斷
            if (sent == 0) {
                std::cerr << "[TCP] 連線已關閉" << std::endl;
            } else {
                perror("send");
            }
            return false;
        }
        remaining -= sent;
        ptr += sent;
    }
    return true;
}

int main(int argc, char* argv[]) {
    // 解析命令行參數
    const char* PCAP_FILE = DEFAULT_PCAP_FILE;
    const char* TCP_TARGET_IP = DEFAULT_TCP_TARGET_IP;
    int TCP_PORT = DEFAULT_TCP_PORT;
    const char* UDP_SOURCE_IP = DEFAULT_UDP_SOURCE_IP;
    int UDP_SOURCE_PORT = DEFAULT_UDP_SOURCE_PORT;
    const char* UDP_REPLAY_IP = DEFAULT_UDP_REPLAY_IP;
    int UDP_REPLAY_PORT = DEFAULT_UDP_REPLAY_PORT;
    const char* MCAST_INTERFACE_IP = DEFAULT_MCAST_INTERFACE_IP;
    bool REAL_TIME_REPLAY = DEFAULT_REAL_TIME_REPLAY;

    // 參數格式: ./replay [pcap_file] [tcp_ip] [tcp_port] [udp_src_ip] [udp_src_port] [udp_replay_ip] [udp_replay_port] [mcast_if_ip] [real_time]
    if (argc >= 2) PCAP_FILE = argv[1];
    if (argc >= 3) TCP_TARGET_IP = argv[2];
    if (argc >= 4) TCP_PORT = std::atoi(argv[3]);
    if (argc >= 5) UDP_SOURCE_IP = argv[4];
    if (argc >= 6) UDP_SOURCE_PORT = std::atoi(argv[5]);
    if (argc >= 7) UDP_REPLAY_IP = argv[6];
    if (argc >= 8) UDP_REPLAY_PORT = std::atoi(argv[7]);
    if (argc >= 9) MCAST_INTERFACE_IP = argv[8];
    if (argc >= 10) REAL_TIME_REPLAY = (std::string(argv[9]) == "true" || std::string(argv[9]) == "1");

    std::cout << "[*] 配置參數:" << std::endl;
    std::cout << "    PCAP 檔案: " << PCAP_FILE << std::endl;
    std::cout << "    TCP 目標: " << TCP_TARGET_IP << ":" << TCP_PORT << std::endl;
    std::cout << "    UDP 來源: " << UDP_SOURCE_IP << ":" << UDP_SOURCE_PORT << std::endl;
    std::cout << "    UDP 回放: " << UDP_REPLAY_IP << ":" << UDP_REPLAY_PORT << std::endl;
    std::cout << "    Multicast 介面: " << MCAST_INTERFACE_IP << std::endl;
    std::cout << "    即時回放: " << (REAL_TIME_REPLAY ? "是" : "否") << std::endl;
    std::cout << std::endl;

    // 註冊訊號處理
    signal(SIGINT, signal_handler);

    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* handle = pcap_open_offline(PCAP_FILE, errbuf);
    if (handle == nullptr) {
        std::cerr << "無法開啟 PCAP 檔案: " << errbuf << std::endl;
        return 1;
    }

    std::cout << "[*] PCAP 開啟成功: " << PCAP_FILE << std::endl;
    
    // 顯示數據鏈路層類型
    int datalink = pcap_datalink(handle);
    std::cout << "[*] 數據鏈路層類型: " << datalink;
    if (datalink == DLT_EN10MB) std::cout << " (Ethernet)";
    else if (datalink == DLT_LINUX_SLL) std::cout << " (Linux cooked v1)";
    else if (datalink == DLT_LINUX_SLL2) std::cout << " (Linux cooked v2)";
    std::cout << std::endl;

    // -------------------------------------------------
    // 1. 建立 UDP Socket (Multicast)
    // -------------------------------------------------
    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sockfd < 0) {
        perror("UDP socket");
        return 1;
    }

    // 設定 Multicast TTL
    int ttl = 2;
    setsockopt(udp_sockfd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    
    // 設定 Multicast 發送介面
    struct in_addr localInterface;
    localInterface.s_addr = inet_addr(MCAST_INTERFACE_IP);
    if (setsockopt(udp_sockfd, IPPROTO_IP, IP_MULTICAST_IF, (char*)&localInterface, sizeof(localInterface)) < 0) {
        perror("Setting local interface");
        std::cerr << "請確認 IP " << MCAST_INTERFACE_IP << " 是否存在於 'ip a' 列表中" << std::endl;
        return 1;
    }
    
    // 開啟 Loopback (讓本機 Server 也能收到)
    int loop = 1;
    setsockopt(udp_sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

    struct sockaddr_in udp_dest_addr;
    memset(&udp_dest_addr, 0, sizeof(udp_dest_addr));
    udp_dest_addr.sin_family = AF_INET;
    udp_dest_addr.sin_addr.s_addr = inet_addr(UDP_REPLAY_IP);  // 回放目標
    udp_dest_addr.sin_port = htons(UDP_REPLAY_PORT);

    // -------------------------------------------------
    // 2. 建立 TCP Socket (Client -> Server)
    // -------------------------------------------------
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in tcp_dest_addr;
    memset(&tcp_dest_addr, 0, sizeof(tcp_dest_addr));
    tcp_dest_addr.sin_family = AF_INET;
    tcp_dest_addr.sin_addr.s_addr = inet_addr(TCP_TARGET_IP);
    tcp_dest_addr.sin_port = htons(TCP_PORT);

    std::cout << "[*] 嘗試連線 TCP " << TCP_TARGET_IP << ":" << TCP_PORT << "..." << std::endl;
    if (connect(tcp_sockfd, (struct sockaddr*)&tcp_dest_addr, sizeof(tcp_dest_addr)) < 0) {
        perror("TCP connect");
        std::cout << "⚠️  連線失敗，將略過 TCP 回放，僅回放 UDP..." << std::endl;
    } else {
        tcp_connected = true;
        std::cout << "✅ TCP 連線成功" << std::endl;
    }

    // -------------------------------------------------
    // 3. 回放迴圈
    // -------------------------------------------------
    struct pcap_pkthdr* header;
    const u_char* packet;
    
    uint64_t start_wall_time = get_current_micros();
    uint64_t start_pcap_time = 0;
    bool first_packet = true;

    long tcp_count = 0;
    long udp_count = 0;
    
    if (REAL_TIME_REPLAY) {
        std::cout << "🚀 開始回放資料 (真實時間模式)..." << std::endl;
    } else {
        std::cout << "🚀 開始回放資料 (全速模式，每 " << FAST_MODE_BATCH_SIZE << " 包休息 " 
                  << FAST_MODE_SLEEP_US/1000.0 << "ms)..." << std::endl;
    }

    while (int returnValue = pcap_next_ex(handle, &header, &packet) >= 0) {
        if (returnValue == 0) continue; // Timeout

        // --- 時間同步邏輯 ---
        uint64_t current_pcap_time = header->ts.tv_sec * 1000000ULL + header->ts.tv_usec;
        if (first_packet) {
            start_pcap_time = current_pcap_time;
            start_wall_time = get_current_micros();
            first_packet = false;
        }

        if (REAL_TIME_REPLAY) {
            uint64_t pcap_diff = current_pcap_time - start_pcap_time;
            uint64_t wall_diff = get_current_micros() - start_wall_time;

            if (pcap_diff > wall_diff) {
                std::this_thread::sleep_for(std::chrono::microseconds(pcap_diff - wall_diff));
            }
        }

        // --- 解析 Ethernet Header ---
        // 判斷是否為 Linux Cooked Capture (tcpdump -i any)
        int ethernet_header_len = 14;
        int datalink = pcap_datalink(handle);
        if (datalink == DLT_LINUX_SLL) {
            ethernet_header_len = 16;  // Linux cooked v1
        } else if (datalink == DLT_LINUX_SLL2) {
            ethernet_header_len = 20;  // Linux cooked v2 (新版)
        }

        const struct ip* ip_header = (struct ip*)(packet + ethernet_header_len);
        
        // 簡單確認版本為 IPv4
        if (ip_header->ip_v != 4) continue;

        char dst_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ip_header->ip_dst), dst_ip, INET_ADDRSTRLEN);

        int ip_header_len = ip_header->ip_hl * 4;
        int protocol = ip_header->ip_p;

        // === TCP 處理 (關鍵修正) ===
        if (protocol == IPPROTO_TCP && tcp_connected) {
            const struct tcphdr* tcp_header = (struct tcphdr*)((u_char*)ip_header + ip_header_len);
            
            int tcp_header_len = tcp_header->doff * 4;
            int total_headers_len = ethernet_header_len + ip_header_len + tcp_header_len;
            
            // 【重要】改用 IP Header 的 Total Length 來計算真實 Payload 長度
            // 這能排除 Ethernet Padding (例如把 54 bytes 補成 64 bytes 的 0x00)
            uint16_t ip_total_len = ntohs(ip_header->ip_len);
            int valid_tcp_payload_len = ip_total_len - ip_header_len - tcp_header_len;

            const u_char* payload = packet + total_headers_len;

            // 過濾目標 IP 和 Port
            if (strcmp(dst_ip, TCP_TARGET_IP) == 0 && ntohs(tcp_header->dest) == TCP_PORT) {
                
                // 只有當真實 payload 大於 0 時才處理 (過濾掉純 ACK 包)
                if (valid_tcp_payload_len > 0) {
                    
                    // 防呆：太短的封包可能是碎片或異常
                    if (valid_tcp_payload_len < 5) {
                        continue;
                    }

                    // --- 應用層協議裁切 (Protocol Trimming) ---
                    // 讀取前 4 bytes (Length)
                    uint32_t body_len_net = 0;
                    memcpy(&body_len_net, payload, 4);
                    uint32_t body_len = ntohl(body_len_net);

                    // 計算協議預期的總長度 (Header 4 + Type 1 + Body)
                    uint32_t expected_total_len = 4 + 1 + body_len;

                    // 比較「真實網路長度」與「協議預期長度」
                    if (valid_tcp_payload_len >= (int)expected_total_len) {
                        // 正常發送 (只發送協議預期的長度，確保不會多發)
                        // 【關鍵修正】使用 send_all 確保所有數據都發送完畢
                        if (send_all(tcp_sockfd, payload, expected_total_len)) {
                            tcp_count++;
                            
                            // 全速模式下的流控：避免瞬間發送過多數據導致 Server 處理不過來
                            if (!REAL_TIME_REPLAY && tcp_count % FAST_MODE_BATCH_SIZE == 0) {
                                std::this_thread::sleep_for(std::chrono::microseconds(FAST_MODE_SLEEP_US));
                            }
                            
                            // 定期顯示進度
                            if (tcp_count % 100 == 0) {
                                std::cout << "\r[TCP] 已發送 " << tcp_count << " 個封包..." << std::flush;
                            }
                        } else {
                            std::cerr << "\n[TCP] 發送失敗 (已發送 " << tcp_count << " 個)，停止 TCP 回放" << std::endl;
                            tcp_connected = false;
                            break;
                        }
                    } else {
                        // 封包殘缺 (Loss)
                        std::cerr << "[TCP] 封包殘缺，略過 (預期 " << expected_total_len << ", 實際 " << valid_tcp_payload_len << ")" << std::endl;
                    }
                }
            }
        }
        // === UDP 處理 ===
        else if (protocol == IPPROTO_UDP) {
            const struct udphdr* udp_header = (struct udphdr*)((u_char*)ip_header + ip_header_len);
            
            // UDP 比較簡單，通常沒有 Padding 問題，或是長度欄位很明確
            int udp_header_len = 8;
            int total_headers_len = ethernet_header_len + ip_header_len + udp_header_len;
            int payload_len = header->caplen - total_headers_len; // 這裡用 caplen 通常沒問題，但用 udphdr->len 更好
            
            // 更精確的做法是讀 udp_header->len
            uint16_t udp_total_len = ntohs(udp_header->len);
            int valid_udp_payload_len = udp_total_len - udp_header_len;

            const u_char* payload = packet + total_headers_len;

            // 過濾 PCAP 中的來源地址，發送到回放目標地址
            if (strcmp(dst_ip, UDP_SOURCE_IP) == 0 && ntohs(udp_header->dest) == UDP_SOURCE_PORT) {
                if (valid_udp_payload_len > 0) {
                    // 發送到回放目標 (239.255.0.1:10000)
                    sendto(udp_sockfd, payload, valid_udp_payload_len, 0, (struct sockaddr*)&udp_dest_addr, sizeof(udp_dest_addr));
                    udp_count++;
                    
                    // 每 200 筆顯示一次進度
                    if (udp_count % 200 == 0) {
                        std::cout << "\r[UDP] 已廣播 " << udp_count << " 筆行情..." << std::flush;
                    }
                }
            }
        }
    }

    std::cout << "\n\n=========================================" << std::endl;
    std::cout << "回放結束。" << std::endl;
    std::cout << "TCP 發送封包數: " << tcp_count << std::endl;
    std::cout << "UDP 發送封包數: " << udp_count << std::endl;
    std::cout << "=========================================" << std::endl;

    pcap_close(handle);
    close(udp_sockfd);
    if (tcp_connected) close(tcp_sockfd);

    return 0;
}