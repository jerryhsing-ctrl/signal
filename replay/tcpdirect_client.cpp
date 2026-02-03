#include <iostream>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

// 引入 TCPDirect (zf) 標頭檔
#include <zf/zf.h>
#include <zf/zf_alts.h>

#define SERVER_IP "10.101.1.102"
#define SERVER_PORT 9004

// 簡單的時間測量工具
using timeType = std::chrono::time_point<std::chrono::system_clock>;
inline timeType get_times() {
    return std::chrono::system_clock::now();
}
inline long long time_diff(timeType start, timeType end) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

int main(int argc, char* argv[]) {
    int ret;
    struct zf_stack* stack;
    struct zf_attr* attr;
    struct zft* zock;
    struct zft_handle* tcp_handle;

    // 1. 初始化 ZF library
    ret = zf_init();
    if (ret < 0) {
        fprintf(stderr, "zf_init failed: %s\n", strerror(-ret));
        return -1;
    }

    // 2. 配置屬性與 Stack
    ret = zf_attr_alloc(&attr);
    if (ret < 0) {
        fprintf(stderr, "zf_attr_alloc failed: %s\n", strerror(-ret));
        return -1;
    }

    // 設定 interface
    zf_attr_set_str(attr, "interface", "enp1s0f0np0");
    // 設定 Alternatives 數量
    zf_attr_set_int(attr, "alt_count", 1);

    ret = zf_stack_alloc(attr, &stack);
    if (ret < 0) {
        fprintf(stderr, "zf_stack_alloc failed: %s\n", strerror(-ret));
        zf_attr_free(attr);
        return -1;
    }

    // 3. 準備連線目標地址
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    // 4. 建立 TCP Zocket (ZFT)
    // 注意：TCPDirect 的 connect 是非阻塞的，或者需要透過 reactor 處理
    // 這裡使用 zft_alloc 建立 socket handle
    ret = zft_alloc(stack, attr, &tcp_handle);
    if (ret < 0) {
        fprintf(stderr, "zft_alloc failed: %s\n", strerror(-ret));
        return -1;
    }

    std::cout << "Connecting to " << SERVER_IP << ":" << SERVER_PORT << "..." << std::endl;

    // 5. 發起連線
    // zft_connect 回傳 0 表示立即成功，EINPROGRESS 表示正在進行
    ret = zft_connect(tcp_handle, (struct sockaddr*)&serv_addr, sizeof(serv_addr), &zock);
    if (ret != 0 && ret != -EINPROGRESS) {
        fprintf(stderr, "zft_connect failed: %s\n", strerror(-ret));
        return -1;
    }

    // 6. 等待連線建立 (Polling Loop)
    while (zft_state(zock) == TCP_SYN_SENT) {
        zf_reactor_perform(stack);
    }

    if (zft_state(zock) != TCP_ESTABLISHED) {
        fprintf(stderr, "Connection failed or closed.\n");
        return -1;
    }

    std::cout << "Connected!" << std::endl;

    // 準備 Alternatives Handle
    zf_althandle alt_handle;
    ret = zf_alternatives_alloc(stack, attr, &alt_handle);
    if (ret < 0) {
        fprintf(stderr, "zf_alternatives_alloc failed: %s\n", strerror(-ret));
        return -1;
    }

    // 7. 傳送與接收迴圈
    int counter = 0;
    char recv_buf[1024];

    while (true) {
        // 準備訊息
        std::string msg = "Hello from TCPDirect client " + std::to_string(counter++);
        
        // --- 發送 (使用 Alternatives) ---
        
        // 1. 預先 Queue (模擬在訊號來臨前準備好資料)
        // 這一步會發生 Copy，但我們把它移到關鍵路徑之外
        struct iovec iov = { (void*)msg.c_str(), msg.length() };
        ret = zft_alternatives_queue(zock, alt_handle, &iov, 1, 0);
        if (ret < 0) {
             if (ret == -EAGAIN) { zf_reactor_perform(stack); continue; }
             fprintf(stderr, "zft_alternatives_queue failed: %s\n", strerror(-ret));
             break;
        }

        // 模擬等待訊號...
        // while (!signal) zf_reactor_perform(stack);

        // 2. 觸發發送 (Zero Copy Trigger)
        auto start = get_times();
        
        ret = zf_alternatives_send(stack, alt_handle);
        
        if (ret < 0) {
            fprintf(stderr, "zf_alternatives_send failed: %s\n", strerror(-ret));
            break;
        }

        std::cout << time_diff(start, get_times()) << " ns (trigger overhead)\n";
        std::cout << "Sent: " << msg << std::endl;

        // --- 接收 ---
        // TCPDirect 是 Event Driven 的，我們需要不斷 polling
        bool received = false;
        auto wait_start = std::chrono::steady_clock::now();

        while (!received) {
            // 驅動 Stack 處理網路封包
            zf_reactor_perform(stack);

            // 定義一個包含 zft_msg 和 iovec 的結構體
            // 根據 ZF 文件，iovec 陣列必須緊跟在 zft_msg 結構之後
            struct {
                struct zft_msg msg;
                struct iovec iov[1];
            } msg_header;

            msg_header.msg.iovcnt = 1;

            // 嘗試讀取 (Zero Copy)
            zft_zc_recv(zock, &msg_header.msg, 0);
            
            if (msg_header.msg.iovcnt > 0) {
                // 有資料
                size_t len = msg_header.iov[0].iov_len;
                if (len > sizeof(recv_buf) - 1) len = sizeof(recv_buf) - 1;
                
                memcpy(recv_buf, msg_header.iov[0].iov_base, len);
                recv_buf[len] = '\0';
                
                std::cout << "Server response: " << recv_buf << std::endl;
                
                // 重要：通知 Stack 我們已經處理完這段資料，可以釋放緩衝區
                zft_zc_recv_done(zock, &msg_header.msg);
                received = true;
            }

            // 簡單的超時機制，避免卡死
            if (std::chrono::steady_clock::now() - wait_start > std::chrono::seconds(2)) {
                // std::cout << "Timeout waiting for response." << std::endl;
                break; 
            }
        }

        // 模擬間隔
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 8. 清理
    if (alt_handle) zf_alternatives_release(stack, alt_handle);
    zft_free(zock);
    zf_stack_free(stack);
    zf_attr_free(attr);
    zf_deinit();

    return 0;
}
