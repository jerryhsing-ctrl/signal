// 10.101.1.102
#include <iostream>
#include <cstring>
#include <string>
#include <chrono>
#include <vector>
#include <algorithm> // for sort
#include <numeric>   // for accumulate
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h> // for cpu pinning

#include <zf/zf.h>

#define SERVER_IP "10.101.1.102"
#define SERVER_PORT 9004
#define TEST_ITERATIONS 100000  // 測試十萬次
#define WARMUP_ITERATIONS 1000  // 暖身次數

// 設定要綁定的 CPU Core ID (請根據你的機器調整，例如 2 或 4)
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

int main(int argc, char* argv[]) {
    // 0. 綁定 CPU，減少 Context Switch
    pin_thread(PIN_CPU_ID);

    int ret;
    struct zf_stack* stack;
    struct zf_attr* attr;
    struct zft* zock;
    struct zft_handle* tcp_handle;

    ret = zf_init();
    if (ret < 0) return -1;

    ret = zf_attr_alloc(&attr);
    if (ret < 0) return -1;

    zf_attr_set_str(attr, "interface", "enp2s0f0np0");

    ret = zf_stack_alloc(attr, &stack);
    if (ret < 0) return -1;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    ret = zft_alloc(stack, attr, &tcp_handle);
    if (ret < 0) return -1;

    std::cout << "Connecting..." << std::endl;
    ret = zft_connect(tcp_handle, (struct sockaddr*)&serv_addr, sizeof(serv_addr), &zock);
    
    // 等待連線
    while (zft_state(zock) == TCP_SYN_SENT) {
        zf_reactor_perform(stack);
    }
    if (zft_state(zock) != TCP_ESTABLISHED) {
        fprintf(stderr, "Connection failed.\n");
        return -1;
    }
    std::cout << "Connected! Starting Benchmark..." << std::endl;

    // 準備資料
    char send_buf[64] = "PING"; 
    int msg_len = 4;
    
    struct {
        struct zft_msg msg;
        struct iovec iov[1];
    } msg_header;

    // 儲存 RTT 結果
    std::vector<long long> latencies;
    latencies.reserve(TEST_ITERATIONS);

    // 總迴圈 (Warmup + Test)
    int total_runs = WARMUP_ITERATIONS + TEST_ITERATIONS;

    for (int i = 0; i < total_runs; ++i) {
        bool is_warmup = (i < WARMUP_ITERATIONS);

        // --- 發送 ---
        auto start = get_times(); // 開始計時
        
        ssize_t sent = zft_send_single(zock, send_buf, msg_len, 0);
        while (sent == -EAGAIN) { // 忙碌等待，不讓 CPU 休息
            zf_reactor_perform(stack);
            sent = zft_send_single(zock, send_buf, msg_len, 0);
        }

        // --- 接收 ---
        bool received = false;
        while (!received) {
            zf_reactor_perform(stack);
            msg_header.msg.iovcnt = 1;
            zft_zc_recv(zock, &msg_header.msg, 0);
            
            if (msg_header.msg.iovcnt > 0) {
                auto end = get_times(); // 停止計時
                
                if (!is_warmup) {
                    latencies.push_back(time_diff(start, end));
                }
                
                zft_zc_recv_done(zock, &msg_header.msg);
                received = true;
            }
        }
    }

    // --- 分析結果 ---
    std::sort(latencies.begin(), latencies.end());
    
    long long sum = std::accumulate(latencies.begin(), latencies.end(), 0LL);
    double avg = (double)sum / latencies.size();
    long long min_lat = latencies.front();
    long long max_lat = latencies.back();
    long long p99 = latencies[(int)(latencies.size() * 0.99)];
    long long p50 = latencies[(int)(latencies.size() * 0.50)];

    std::cout << "\n=== TCPDirect Benchmark Results (" << TEST_ITERATIONS << " samples) ===\n";
    std::cout << "Min : " << min_lat << " ns\n";
    std::cout << "P50 : " << p50 << " ns\n";
    std::cout << "Avg : " << avg << " ns\n";
    std::cout << "P99 : " << p99 << " ns\n";
    std::cout << "Max : " << max_lat << " ns\n";

    // 清理
    zft_free(zock);
    zf_stack_free(stack);
    zf_attr_free(attr);
    zf_deinit();
    return 0;
}
// #include <iostream>
// #include <cstring>
// #include <string>
// #include <chrono>
// #include <thread>
// #include <vector>
// #include <arpa/inet.h>
// #include <netinet/in.h>
// #include <netinet/tcp.h>

// // 引入 TCPDirect (zf) 標頭檔
// #include <zf/zf.h>

// #define SERVER_IP "10.101.1.102"
// #define SERVER_PORT 9004

// // 使用 steady_clock 進行高精度計時
// using timeType = std::chrono::time_point<std::chrono::steady_clock>;

// inline timeType get_times() {
//     return std::chrono::steady_clock::now();
// }

// inline long long time_diff(timeType start, timeType end) {
//     return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
// }

// int main(int argc, char* argv[]) {
//     int ret;
//     struct zf_stack* stack;
//     struct zf_attr* attr;
//     struct zft* zock;
//     struct zft_handle* tcp_handle;

//     // 1. 初始化 ZF library
//     ret = zf_init();
//     if (ret < 0) {
//         fprintf(stderr, "zf_init failed: %s\n", strerror(-ret));
//         return -1;
//     }

//     // 2. 配置屬性與 Stack
//     ret = zf_attr_alloc(&attr);
//     if (ret < 0) {
//         fprintf(stderr, "zf_attr_alloc failed: %s\n", strerror(-ret));
//         return -1;
//     }

//     // 設定 interface
//     zf_attr_set_str(attr, "interface", "enp2s0f0np0");

//     ret = zf_stack_alloc(attr, &stack);
//     if (ret < 0) {
//         fprintf(stderr, "zf_stack_alloc failed: %s\n", strerror(-ret));
//         zf_attr_free(attr);
//         return -1;
//     }

//     // 3. 準備連線目標地址
//     struct sockaddr_in serv_addr;
//     memset(&serv_addr, 0, sizeof(serv_addr));
//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_port = htons(SERVER_PORT);
//     inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

//     // 4. 建立 TCP Zocket (ZFT)
//     ret = zft_alloc(stack, attr, &tcp_handle);
//     if (ret < 0) {
//         fprintf(stderr, "zft_alloc failed: %s\n", strerror(-ret));
//         return -1;
//     }

//     std::cout << "Connecting to " << SERVER_IP << ":" << SERVER_PORT << "..." << std::endl;

//     // 5. 發起連線
//     ret = zft_connect(tcp_handle, (struct sockaddr*)&serv_addr, sizeof(serv_addr), &zock);
//     if (ret != 0 && ret != -EINPROGRESS) {
//         fprintf(stderr, "zft_connect failed: %s\n", strerror(-ret));
//         return -1;
//     }

//     // 6. 等待連線建立
//     while (zft_state(zock) == TCP_SYN_SENT) {
//         zf_reactor_perform(stack);
//     }

//     if (zft_state(zock) != TCP_ESTABLISHED) {
//         fprintf(stderr, "Connection failed or closed.\n");
//         return -1;
//     }

//     std::cout << "Connected!" << std::endl;

//     // 7. 傳送與接收迴圈
//     int counter = 0;
    
//     // 預先準備好發送緩衝區，避免在計時過程中進行 new/string 操作
//     char send_buf[1024]; 

//     // 預先宣告接收結構，避免 loop 內的 stack allocation
//     struct {
//         struct zft_msg msg;
//         struct iovec iov[1];
//     } msg_header;

//     while (true) {
//         // [準備數據] 這一步不計入 RTT，所以在計時前完成
//         int msg_len = snprintf(send_buf, sizeof(send_buf), "Hello TCPDirect %d", counter++);

//         // =========== RTT 計時開始 ===========
//         // 從「發送」開始計時
//         auto start = get_times();
        
//         // 發送數據
//         ssize_t sent = zft_send_single(zock, send_buf, msg_len, 0);
        
//         if (sent < 0) {
//             if (sent == -EAGAIN) {
//                 zf_reactor_perform(stack);
//                 continue; // 重試
//             }
//             fprintf(stderr, "zft_send failed: %s\n", strerror(-sent));
//             break;
//         }

//         // --- 接收迴圈 (Polling) ---
//         bool received = false;
//         long long rtt_ns = 0;

//         while (!received) {
//             // 驅動網卡
//             zf_reactor_perform(stack);

//             msg_header.msg.iovcnt = 1; // 告訴 ZF 我們可以接收 1 個 iovec

//             // 嘗試 Zero Copy 接收
//             // 這裡拿到的是指向 ZF 內部緩衝區的指標，尚未發生複製
//             zft_zc_recv(zock, &msg_header.msg, 0);
            
//             if (msg_header.msg.iovcnt > 0) {
//                 // =========== RTT 計時結束 ===========
//                 // 一旦偵測到數據到達，立刻停止計時
//                 auto end = get_times();
//                 rtt_ns = time_diff(start, end);
                
//                 // [Zero Copy 驗證]
//                 // 這裡我們直接讀取 msg_header.iov[0].iov_base 
//                 // 我們移除了原本的 memcpy(recv_buf, ...)
                
//                 // 若需要列印內容，使用指針即可 (注意：這是 Direct Memory Access)
//                 // 為了不影響 RTT 數字，列印移到下方
                
//                 // 通知 Stack 我們用完了，可以釋放這個封包的記憶體
//                 zft_zc_recv_done(zock, &msg_header.msg);
                
//                 received = true;
//             }
//         }

//         // 這裡已經是非關鍵路徑，可以安心做 IO 輸出
//         std::cout << "RTT: " << rtt_ns << " ns" << std::endl;

//         // 模擬間隔
//         std::this_thread::sleep_for(std::chrono::seconds(1));
//     }

//     // 8. 清理
//     zft_free(zock);
//     zf_stack_free(stack);
//     zf_attr_free(attr);
//     zf_deinit();

//     return 0;
// }