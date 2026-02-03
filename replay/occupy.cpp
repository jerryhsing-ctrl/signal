#define _GNU_SOURCE // 必須定義這個才能使用 CPU Affinity 相關函數
#include <sched.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <iostream>

using namespace std;

// --- 以下為圖片中的函數實作 ---

// 設定當前執行緒為 SCHED_FIFO 模式，並指定優先級 (1-99)
void set_fifo_priority(int priority) {
    struct sched_param param;
    param.sched_priority = priority;

    pthread_t current_thread = pthread_self();
    // 嘗試將排程策略改為 SCHED_FIFO
    if (pthread_setschedparam(current_thread, SCHED_FIFO, &param) != 0) {
        perror("pthread_setschedparam failed (Did you run with sudo?)");
        // 為了演示，這裡不強制退出，但在生產環境這通常是致命錯誤
    }
}

void pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    // 將當前執行緒綁定到指定 Core
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("pthread_setaffinity_np failed");
    }
}

void* worker_thread(void* arg) {
    // 取得傳入的 core_id
    int core_id = *(int*)arg;

    // 1. 先綁定 CPU (確保在隔離核心上運行)
    printf("core id is %d\n", core_id);
    pin_thread_to_core(core_id);
    
    printf("Thread launched on Core %d\n", core_id);

    // 2. 設定 SCHED_FIFO 優先級 (HFT 通常設為最高 99)
    // 注意：這需要 root 權限或 CAP_SYS_NICE 能力
    set_fifo_priority(99);

    // 3. 再進入無限迴圈 (佔用 CPU)
    // 為了避免被編譯器優化掉，加入簡單的運算或 volatile
    while(1) {
        // HFT Logic... 100% CPU Usage
        // 由於設定了 nohz_full 和 rcu_nocbs，這裡不需要 sleep 也不會當機
        
        // 這裡放一個簡單的指令防止空迴圈被編譯器優化成 pause
        __asm__ __volatile__ ("nop"); 
        // std::cout << " hi\n";
        int *p = new int[1000];
        delete[] p;
        // sleep(1); // 模擬一些工作負載
    }
    
    return NULL;
}

// --- 主程式 ---

int main() {
    alarm(60);
    const int START_CORE = 1;
    const int END_CORE = 15;
    int num_threads = END_CORE - START_CORE + 1;

    std::vector<pthread_t> threads;
    std::vector<int> core_ids; // 用來儲存參數，確保記憶體位置有效

    for(int i = START_CORE; i <= END_CORE; ++i) {
        core_ids.push_back(i); // 存入 vector
    }

    printf("Starting %d threads on cores %d to %d...\n", num_threads, START_CORE, END_CORE);

    for (size_t i = 0; i < core_ids.size(); ++i) {
        
        pthread_t thread;
        // 建立 Thread，傳入 core_ids 中對應元素的位址
        if (pthread_create(&thread, NULL, worker_thread, &core_ids[i]) != 0) {
            perror("Failed to create thread");
            return 1;
        }
        threads.push_back(thread);
    }

    // 等待所有 Thread (實際上這些 Thread 是死迴圈，主程式會卡在這裡直到被強制結束)
    for (auto& t : threads) {
        pthread_join(t, NULL);
    }

    return 0;
}