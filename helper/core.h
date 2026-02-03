#pragma once

#include <iostream>
#include <string>
#include <cstring>   // for strerror
#include <thread>
#include <pthread.h> // for pthread_self, pthread_setaffinity_np
#include <sched.h>   // for CPU_SET, CPU_ZERO
#include <vector>
#include <fstream>
#include <dirent.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <signal.h>
#include "errorLog.h"
/**
 * 內部函式：綁定核心
 */
#include <atomic> // 建議在大型專案中使用 atomic 以確保執行緒安全

// inline 關鍵字告訴連結器：即使這個變數出現在多個翻譯單元，也請合併成同一個
inline std::atomic<int> g_sharedCounter{3};
inline bool pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    int result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        std::cerr << "❌ [Error] 無法綁定到 Core " << core_id 
                  << ": " << strerror(result) << std::endl;
        return false;
    }
    return true;
}

inline void restrict_thread_to_cores(const std::vector<int>& cores) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int core : cores) {
        CPU_SET(core, &cpuset);
    }
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) std::cerr << "Error setting affinity range!" << std::endl;
}

/**
 * 內部函式：設定名稱 (Linux 上限 15 字元)
 */
inline void set_thread_name(const std::string& name) {
    // 截斷為 15 字元以符合 Linux pthread 限制
    std::string short_name = name.substr(0, 15);
    pthread_setname_np(pthread_self(), short_name.c_str());
}


inline bool set_realtime_priority() {
    struct sched_param param;
    param.sched_priority = 99; // 設定為最高 99 (1-99)

    // SCHED_FIFO: 先進先出，霸佔 CPU 直到 yield 或 I/O
    // 0 代表設定自己 (Current Process)
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
		errorLog("設定權限失敗");
        return false;
    }
    
    std::cout << "✅ [Success] Process is now SCHED_FIFO (Priority 99)" << std::endl;
    return true;
}

#include <dirent.h>
#include <vector>

inline bool set_fifo_priority(int priority) {

    struct sched_param param;

    param.sched_priority = priority;



    pthread_t current_thread = pthread_self();

    // 嘗試將排程策略改為 SCHED_FIFO

    if (pthread_setschedparam(current_thread, SCHED_FIFO, &param) != 0) {

        errorLog("設定權限失敗");

    // 為了演示，這裡不強制退出，但在生產環境這通常是致命錯誤
        return false;
    }
    std::cout << "✅ [Success] Process is now SCHED_FIFO (Priority 99)" << std::endl;
    return true;
}

/**
 * 設定綁定在特定 Core 上的所有 Thread 為 SCHED_FIFO
 * @param target_cores 目標 Core ID 列表
 * @param priority 優先級 (1-99)
 * @param target_name 目標 Thread 名稱 (部分匹配)
 */
inline void set_fifo_priority_for_cores(const std::vector<int>& target_cores, int priority, const std::string& target_name) {
    DIR* dir = opendir("/proc/self/task");
    if (!dir) return;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        int tid = std::stoi(ent->d_name);
        
        // 讀取 thread 名稱
        std::string comm_path = "/proc/self/task/" + std::string(ent->d_name) + "/comm";
        std::ifstream comm_file(comm_path);
        std::string thread_name;
        if (comm_file.is_open()) {
            std::getline(comm_file, thread_name);
            // 去除可能的換行符
            if (!thread_name.empty() && thread_name.back() == '\n') {
                thread_name.pop_back();
            }
        }
        
        // 檢查名稱是否包含目標字串
        if (thread_name.find(target_name) == std::string::npos) {
            continue;
        }

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        
        // 取得該 thread 的 affinity
        if (sched_getaffinity(tid, sizeof(cpu_set_t), &cpuset) == 0) {
            bool is_target = false;
            
            // 檢查該 thread 是否有綁定在目標 core 上
            // 這裡的邏輯是：只要它"可以"在目標 core 上跑，我們就設定它
            // 如果要更嚴格(只綁定在該core)，可以用 CPU_COUNT(&cpuset) == 1 判斷
            for (int core : target_cores) {
                if (CPU_ISSET(core, &cpuset)) {
                    is_target = true;
                    break;
                }
            }

            if (is_target) {
                struct sched_param param;
                param.sched_priority = priority;
                if (sched_setscheduler(tid, SCHED_FIFO, &param) == 0) {
                    std::cout << "✅ Set TID " << tid << " Name: " << thread_name << " (Affinity includes target cores) to SCHED_FIFO priority " << priority << std::endl;
                } else {
                    std::cerr << "❌ Failed to set TID " << tid << " Name: " << thread_name << " to SCHED_FIFO: " << strerror(errno) << std::endl;
                }
            }
        }
    }
    closedir(dir);
}

// 必須在 main 初始化時呼叫一次，註冊訊號處理器
inline void install_thread_cancel_handler() {
    struct sigaction sa;
    sa.sa_handler = [](int) {
        // SYS_exit 只結束呼叫的 thread，不會結束整個 process (區別於 exit_group)
        syscall(SYS_exit, 0);
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);
}

inline void cancel_thread_on_cores(const std::vector<int>& target_cores, const std::string& target_name) {
    DIR* dir = opendir("/proc/self/task");
    if (!dir) return;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        int tid = std::stoi(ent->d_name);
        
        // 讀取 thread 名稱
        std::string comm_path = "/proc/self/task/" + std::string(ent->d_name) + "/comm";
        std::ifstream comm_file(comm_path);
        std::string thread_name;
        if (comm_file.is_open()) {
            std::getline(comm_file, thread_name);
            // 去除可能的換行符
            if (!thread_name.empty() && thread_name.back() == '\n') {
                thread_name.pop_back();
            }
        }
        
        // 檢查名稱是否包含目標字串
        if (thread_name.find(target_name) == std::string::npos) {
            continue;
        }

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        
        // 取得該 thread 的 affinity
        if (sched_getaffinity(tid, sizeof(cpu_set_t), &cpuset) == 0) {
            bool is_target = false;
            
            for (int core : target_cores) {
                if (CPU_ISSET(core, &cpuset)) {
                    is_target = true;
                    break;
                }
            }

            if (is_target) {
                // 發送 SIGUSR2 給特定 TID
                // 需搭配 install_thread_cancel_handler 使用
                int ret = syscall(SYS_tgkill, getpid(), tid, SIGUSR2);
                if (ret == 0) {
                    std::cout << "✅ Sent SIGUSR2 to cancel TID " << tid << " Name: " << thread_name << std::endl;
                } else {
                    std::cerr << "❌ Failed to cancel TID " << tid << ": " << strerror(errno) << std::endl;
                }
            }
        }
    }
    closedir(dir);
}