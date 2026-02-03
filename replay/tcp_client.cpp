#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h> // TCP_NODELAY
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm> // for sort
#include <numeric>   // for accumulate
#include <pthread.h> // for cpu pinning

#define PORT 9004
#define SERVER_IP "10.101.1.102" // 請確認此 IP 是否正確
#define BUFFER_SIZE 1024

// 設定測試次數
#define TEST_ITERATIONS 100000 
#define WARMUP_ITERATIONS 1000

// 設定 CPU 核心 (請確保與 TCPDirect 版本使用相同的 Core ID，或至少不衝突的 Core)
#define PIN_CPU_ID 2

using timeType = std::chrono::time_point<std::chrono::steady_clock>;

inline timeType get_times() {
    return std::chrono::steady_clock::now();
}

inline long long time_diff(timeType start, timeType end) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

// 綁定 CPU 核心
void pin_thread(int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    pthread_t current_thread = pthread_self();
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "Error calling pthread_setaffinity_np" << std::endl;
    } else {
        std::cout << "Thread pinned to CPU " << cpu_id << std::endl;
    }
}

int main() {
    // 0. 綁定 CPU
    pin_thread(PIN_CPU_ID);

    int sock = 0;
    struct sockaddr_in serv_addr;
    
    char send_buf[BUFFER_SIZE] = "PING"; // 固定內容，避免 snprintf 開銷
    char recv_buf[BUFFER_SIZE] = {0};
    int msg_len = 4; // "PING" 的長度

    // 1. 建立 Socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cout << "\n Socket creation error \n";
        return -1;
    }

    // 2. [重要] 設定 TCP_NODELAY
    int flag = 1;
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0) {
        std::cout << "Could not set TCP_NODELAY\n";
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        std::cout << "\nInvalid address/ Address not supported \n";
        return -1;
    }

    // 3. 連線
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cout << "\nConnection Failed \n";
        return -1;
    }

    std::cout << "Connected! Starting Benchmark..." << std::endl;

    // 準備統計容器
    std::vector<long long> latencies;
    latencies.reserve(TEST_ITERATIONS);

    int total_runs = WARMUP_ITERATIONS + TEST_ITERATIONS;

    // 4. 開始迴圈測試
    for (int i = 0; i < total_runs; ++i) {
        bool is_warmup = (i < WARMUP_ITERATIONS);

        // =========== RTT 計時開始 ===========
        auto start = get_times();

        // 發送 (Blocking Send)
        // 在標準 Socket 中，send 通常是將資料拷貝到 Kernel Buffer 就返回
        ssize_t sent_len = send(sock, send_buf, msg_len, 0);
        
        if (sent_len < 0) {
             std::cout << "Send failed" << std::endl;
             break;
        }

        // 接收 (Blocking Read)
        // 程式會在此卡住(Sleep)，直到 Kernel 收到封包並喚醒 Process
        // 這包含了 Context Switch (Kernel -> User) 的開銷
        int valread = read(sock, recv_buf, BUFFER_SIZE);

        // =========== RTT 計時結束 ===========
        auto end = get_times();

        if (valread > 0) {
            if (!is_warmup) {
                latencies.push_back(time_diff(start, end));
            }
        } else {
            std::cout << "Connection closed or error" << std::endl;
            break;
        }
        
        // 這裡移除了 sleep，讓它全速跑
    }

    // 5. 分析結果
    std::sort(latencies.begin(), latencies.end());
    
    long long sum = std::accumulate(latencies.begin(), latencies.end(), 0LL);
    double avg = (double)sum / latencies.size();
    long long min_lat = latencies.front();
    long long max_lat = latencies.back();
    long long p99 = latencies[(int)(latencies.size() * 0.99)];
    long long p50 = latencies[(int)(latencies.size() * 0.50)];

    std::cout << "\n=== Standard Socket Benchmark Results (" << TEST_ITERATIONS << " samples) ===\n";
    std::cout << "Min : " << min_lat << " ns\n";
    std::cout << "P50 : " << p50 << " ns\n";
    std::cout << "Avg : " << avg << " ns\n";
    std::cout << "P99 : " << p99 << " ns\n";
    std::cout << "Max : " << max_lat << " ns\n";

    close(sock);
    return 0;
}

// #include <iostream>
// #include <cstring>
// #include <unistd.h>
// #include <arpa/inet.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <netinet/tcp.h> // 為了 TCP_NODELAY
// #include <thread>
// #include <chrono>
// #include <cstdio> // 為了 snprintf

// #define PORT 9004
// #define SERVER_IP "10.101.1.102"
// #define BUFFER_SIZE 1024

// // 與 TCPDirect 版本保持一致，使用 steady_clock
// using timeType = std::chrono::time_point<std::chrono::steady_clock>;

// inline timeType get_times() {
//     return std::chrono::steady_clock::now();
// }

// inline long long time_diff(timeType start, timeType end) {
//     return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
// }

// int main() {
//     int sock = 0;
//     struct sockaddr_in serv_addr;
    
//     // 預先配置 User Space 的記憶體，避免在計時期間分配
//     char send_buf[BUFFER_SIZE] = {0};
//     char recv_buf[BUFFER_SIZE] = {0};

//     // 1. 建立 Socket
//     if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
//         std::cout << "\n Socket creation error \n";
//         return -1;
//     }

//     // [重要優化] 關閉 Nagle 演算法 (TCP_NODELAY)
//     // 如果不關閉，標準 Kernel Stack 會嘗試合併小封包，導致 RTT 暴增 (例如從 20us 變成 40ms)
//     // TCPDirect 通常不受此限，為了公平比較必須設定。
//     int flag = 1;
//     int result = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
//     if (result < 0) {
//         std::cout << "Could not set TCP_NODELAY\n";
//     }

//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_port = htons(PORT);

//     // 轉換 IP 地址
//     if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
//         std::cout << "\nInvalid address/ Address not supported \n";
//         return -1;
//     }

//     // 2. 連線 (Connect)
//     if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
//         std::cout << "\nConnection Failed \n";
//         return -1;
//     }

//     std::cout << "Connected to server!" << std::endl;

//     // 3. 持續發送訊息
//     int counter = 0;
//     while (true) {
//         // [準備數據] 在計時前完成，不計入 RTT
//         // 這裡避免使用 std::string 的動態記憶體配置
//         int msg_len = snprintf(send_buf, BUFFER_SIZE, "Hello Standard Socket %d", counter++);

//         // =========== RTT 計時開始 ===========
//         auto start = get_times();

//         // 發送
//         ssize_t sent_len = send(sock, send_buf, msg_len, 0);
        
//         if (sent_len < 0) {
//              std::cout << "Send failed" << std::endl;
//              break;
//         }

//         // 接收 (這會 Block 直到收到資料)
//         // 注意：標準 Socket 無法像 TCPDirect 那樣做到完全 Zero Copy (Kernel -> User 必須拷貝)
//         // 但我們這裡直接寫入 recv_buf，不再做額外的 string copy
//         int valread = read(sock, recv_buf, BUFFER_SIZE);

//         // =========== RTT 計時結束 ===========
//         // 一收到資料立刻停止
//         auto end = get_times();

//         if (valread > 0) {
//             // 在計時結束後才進行邏輯處理或列印
//             recv_buf[valread] = '\0'; // 確保字串結尾
            
//             long long rtt = time_diff(start, end);
//             std::cout << "RTT: " << rtt << " ns" << std::endl;
//             // std::cout << "Server response: " << recv_buf << std::endl;
//         } else if (valread == 0) {
//             std::cout << "Server closed connection" << std::endl;
//             break;
//         } else {
//             std::cout << "Read error" << std::endl;
//             break;
//         }

//         std::this_thread::sleep_for(std::chrono::seconds(1));
//     }

//     close(sock);
//     return 0;
// }