
#include "UDPSession.h"
#include "PackBCD.h"
#include "timeUtil.h"
#include "errorLog.h"
// #include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>        // for iovec
#include <linux/net_tstamp.h> // 核心時間戳定義
#include <linux/errqueue.h>   // 錯誤隊列定義
#include <cstring>
#include <iostream>
#include <chrono>
#include "core.h"
#include <time.h>
#include <stdlib.h> // for exit()

#define MINLEN 32
#define HEADER		0x1b
#define ENDFISRT	0x0d
#define ENDSECOND	0x0a
UDPSession::UDPSession(const char *market_, UdpPkgCallback cb, int idx):sock_fd(-1), running(false), market(market_), rcvPkcCallback(std::move(cb)), isSetFilter(false), idx(idx)
{	
}
//---------------------------------------------------------------------------
UDPSession::~UDPSession(void)
{
	CloseSocket();
}
//---------------------------------------------------------------------------
 void UDPSession::SetFilter(std::list<unsigned short> flist)
 {	 
	formatList = flist;
	isSetFilter = true;
 }
//---------------------------------------------------------------------------

bool UDPSession::Open(const char* bind_ip, const char* group_ip, int port) {
	
	printf("UDPSession::Open(%s, %s, %i).\n", bind_ip, group_ip, port);	
	
	int flag = 1;
	struct ip_mreq mreq;
	memset(&srv_addr, 0, sizeof(srv_addr));
	memset(&cli_addr, 0, sizeof(cli_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(port);
	// 綁定到多播組地址，這樣只有加入該多播組的封包才會被接收
	// 避免 SO_REUSEADDR 導致不同多播組的 socket 都收到封包
	srv_addr.sin_addr.s_addr = inet_addr(group_ip);
	printf("Binding to %s:%d for multicast reception\n", group_ip, port);


	if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("socket create() failed\n");
		return false;
	}
	else
		printf("socket create() success\n");
	if (setsockopt(sock_fd, SOL_SOCKET, SO_TIMESTAMPNS, &flag, sizeof(flag)) < 0) {
        printf("setsockopt SO_TIMESTAMPNS failed\n");
        return false;
    }
	if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
		printf("socket set reuse failed\n");
		return false;
	}
	else 
		printf("socket set reuse success\n");
	if (bind(sock_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
		printf("socket bind() failed\n");
		return false;
	}
	else
		printf("socket bind() success\n");
	
	// 設定要加入的 multicast group
	if (inet_aton(group_ip, &mreq.imr_multiaddr) < 0) {
        printf("inet_aton() mreq failed for group %s\n", group_ip);
		return false;
    }
	// 指定從哪個網卡接收 multicast
	// mreq.imr_interface.s_addr = inet_addr(bind_ip);
	if (inet_aton(bind_ip, &mreq.imr_interface) == 0) { // 0 代表失敗
        printf("Invalid bind_ip (interface): %s\n", bind_ip);
        return false;
    }

	printf("Joining multicast group %s on interface %s\n", group_ip, bind_ip);
	if (setsockopt(sock_fd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)) < 0) {
		printf("socket joining multiCast group failed\n");
		return false;
	}
	else
		printf("socket joining multiCast group success\n");
	
	printf("LINKREADY\n");	
	
	running = true;
	// recv_thread = std::thread(&UDPSession::ReceiveLoop, this);
	printf("Create recv_thread Thread.\n");
	
	return true;
}


//---------------------------------------------------------------------------
void UDPSession::ReceiveLoop() {
	// for (int i = 0; i < 5; i++) {
	// 	std::cout << " cnt = " << g_sharedCounter << '\n';
	// }
	// setup_thread(core, "internet_thread");
	
	// debug_core_3_direct();

	unsigned char buffer[2048];	
	
	printf("exec ReceiveLoop Thread.\n");
	
	int cli_len = sizeof(cli_addr);
	int recv_len;
		
	while (running) {
		recv_len = recvfrom(sock_fd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, (socklen_t *)&cli_len);									
		timeType timeStamp = get_times();
		if (recv_len > 0) {
			ParsePackage(buffer, recv_len, timeStamp);
		} else if (recv_len < 0) {
			errorLog("UDPSession::ReceiveLoop, recvfrom 錯誤");
		}
	}
}
/*------------------------------------------------------------------------------------------------*/

// 這邊邏輯條件要再檢查
void UDPSession::ParsePackage(unsigned char *pkgMsg, int pkgLength, timeType timeStamp)
{
	
    int  index=0, length = 0;    	
	unsigned short formatId = 0;	
	//printf("ParsePackage ParsePackage=%i.\n", pkgLength);  
	// 變動長度32 ~ 131 Bytes 
	// int cnt = 0;
    while (index < pkgLength)
    {
		
		if(pkgMsg[index]==HEADER)
		{
				// auto time1 = get_times();
				// length = PackBCD2Int((unsigned char*)&pkgMsg[index+1], 2);   
				length = PackBCD2(&pkgMsg[index + 1]);               
				// auto time2 = get_times();
				// std::cout << " 解析長度花費 " << time_diff(time1, time2) << " 奈秒\n";
				formatId = pkgMsg[index + 4];
				if (formatId != 6)
					return;
				// cout << " format6\n";
				// cout << "index " << index << " pkgLength " << pkgLength << " " << PackBCD4(&pkgMsg[index + 6]) << " " << PackBCD6(&pkgMsg[index + 16]) << '\n';
				if (pkgLength >= index+length && pkgMsg[index+length - 2] == ENDFISRT && pkgMsg[index+length - 1] == ENDSECOND) { 
					// if (!isSetFilter || std::binary_search(formatList.begin(), formatList.end(), formatId))
					// {						
						// unsigned char pkbBuffer[length];
						// memcpy(pkbBuffer, pkgMsg+index, length);
						// cout << "idx " << idx << '\n';
						if (rcvPkcCallback) rcvPkcCallback(market, pkgMsg + index, length, timeStamp, idx);
					
					index+=length;	
				}
				else {
					errorLog("UDPSession::ParsePackage, 封包結尾錯誤");
				}	
		}
		else                
		{               
			// printf("*%i", index);
			errorLog("UDPSession::ParsePackage, 找不到封包標頭");                    
			index++;    
		}
		// cout << cnt << '\n';
    }    
     //printf("------------------------------------------------------------------------------------------------------\n");
}

//---------------------------------------------------------------------------

void UDPSession::CloseSocket() {
	if (running) {
		running = false;
		if (recv_thread.joinable()) {
			recv_thread.join();
		}
	}

	if (sock_fd >= 0) {
		close(sock_fd);
		sock_fd = -1;
	}
}
//---------------------------------------------------------------------------
// void UDPSession::ReceiveLoop() {
// 	unsigned char buffer[2048];	
	
// 	printf("exec ReceiveLoop Thread.\n");
	
// 	int cli_len = sizeof(cli_addr);
// 	int recv_len;
		
// 	while (running) {
// 		recv_len = recvfrom(sock_fd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, (socklen_t *)&cli_len);									
// 		timeType timeStamp = get_times();
// 		if (recv_len > 0) {
// 			ParsePackage(buffer, recv_len, timeStamp);
// 		} else if (recv_len < 0) {
// 			errorLog("UDPSession::ReceiveLoop, recvfrom 錯誤");
// 		}
// 	}
// }
// void UDPSession::ReceiveLoop() {
//     printf("exec ReceiveLoop Thread (Hardware Timestamp Mode).\n");

//     // 1. 準備接收資料的 Buffer
//     unsigned char buffer[2048];
//     struct iovec iov;
//     iov.iov_base = buffer;
//     iov.iov_len = sizeof(buffer);

//     // 2. 準備接收 Control Message (用來存放 Kernel 傳回來的時間戳)
//     // 必須足夠大以容納 cmsghdr 和 timespec 陣列
//     char ctrl_buf[1024]; 
    
//     // 3. 準備 Message Header
//     struct msghdr msg;
//     // 初始化 msg 結構 (只需要做一次大架構，但在迴圈內要重置長度)
//     memset(&msg, 0, sizeof(msg));
//     msg.msg_iov = &iov;
//     msg.msg_iovlen = 1;
//     msg.msg_control = ctrl_buf;
//     msg.msg_controllen = sizeof(ctrl_buf);
//     msg.msg_name = &cli_addr; 
//     msg.msg_namelen = sizeof(cli_addr);

//     while (running) {
//         // [重要] 每次 recvmsg 之前，必須重置 msg_controllen
//         // 因為 recvmsg 返回後會把這個值改成"實際收到"的長度
//         // 如果不重置，下次呼叫可能會因為空間不足而拿不到時間戳
//         msg.msg_controllen = sizeof(ctrl_buf);

//         // -------------------------------------------------------------
//         // A. 接收資料 (Blocking / Polling based on Onload settings)
//         // -------------------------------------------------------------
//         int recv_len = recvmsg(sock_fd, &msg, 0);

//         // -------------------------------------------------------------
//         // B. 立即打點 (軟體時間 T_After)
//         // -------------------------------------------------------------
//         timeType app_time = get_times(); 

//         if (recv_len > 0) {
//             // ---------------------------------------------------------
//             // C. 解析硬體時間戳 (T_HW)
//             // ---------------------------------------------------------
//             struct timespec hw_ts = {0, 0};
//             bool found_hw_ts = false;
//             struct cmsghdr *cmsg;

//             // 遍歷 Control Message 尋找 SO_TIMESTAMPING
//             for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
//                 if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPING) {
//                     // CMSG_DATA 裡面包含 3 個 timespec 結構：
//                     // ts[0]: Software Timestamp (軟體打的，不準)
//                     // ts[1]: Hardware Timestamp (經過轉換的系統時間，常用)
//                     // ts[2]: Hardware Timestamp (Raw NIC Clock，最原始) -> Solarflare 推薦看這個
//                     struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);
                    
//                     // 這裡取 ts[2] (Raw Hardware)
//                     hw_ts = ts[2]; 
                    
//                     // 防呆：如果 ts[2] 是空的 (0)，嘗試退回用 ts[1]
//                     if (hw_ts.tv_sec == 0 && hw_ts.tv_nsec == 0) {
//                         hw_ts = ts[1];
//                     }
                    
//                     if (hw_ts.tv_sec != 0) {
//                         found_hw_ts = true;
//                     }
//                     break;
//                 }
//             }

//             // ---------------------------------------------------------
//             // D. 計算 Latency = (T_After - T_HW)
//             // ---------------------------------------------------------
//             if (found_hw_ts) {
//                 // 1. 把 C 語言的硬體時間 (struct timespec) 轉成 -> 總奈秒數 (long)
//                 long hw_ns_total = (hw_ts.tv_sec * 1000000000L) + hw_ts.tv_nsec;

//                 // 2. 把 C++ 的軟體時間 (std::chrono) 轉成 -> 總奈秒數 (long)
//                 // 原理：取得自 Epoch 以來的時間長度 -> 轉成奈秒單位 -> 取出數值
//                 auto app_duration = app_time.time_since_epoch();
//                 long app_ns_total = std::chrono::duration_cast<std::chrono::nanoseconds>(app_duration).count();

//                 // 3. 相減得到延遲
//                 long latency_ns = app_ns_total - hw_ns_total;

//                 // 4. 印出結果
//                 // 注意：如果沒做時鐘同步 (phc2sys)，這裡可能會出現負數，屬正常現象
//                 // if (latency_ns > 5000 || latency_ns < -5000) { 
//                      // std::cout << "Latency: " << latency_ns << " ns" << std::endl;
//                      // 建議用 printf 比較不會被 buffer 卡住
//                      printf("[LATENCY] %ld ns (App: %ld - HW: %ld)\n", latency_ns, app_ns_total, hw_ns_total);
//                 // }
//             }
//             else {
//                 // 如果一直印這個，代表 Open() 裡的 setsockopt 沒成功，或者 onload 沒掛上去
//                 // printf("[INFO] No HW Timestamp found.\n");
//             }

//             // ---------------------------------------------------------
//             // E. 處理封包 (傳入 buffer 指標，避免 memcpy)
//             // ---------------------------------------------------------
//             // 建議將 hw_ts 也傳入，因為它是最準確的"市場發生時間"
//             ParsePackage(buffer, recv_len, app_time);

//         } else if (recv_len < 0) {
//             // 忽略 EAGAIN (資源暫時不可用)，其他則是真錯誤
//             if (errno != EAGAIN && errno != EWOULDBLOCK) {
//                 errorLog("recvmsg failed");
//             }
//         }
//     }
// }
// void UDPSession::ReceiveLoop() {
//     printf("exec ReceiveLoop Thread (Debug Mode).\n");

//     unsigned char buffer[2048];
//     struct iovec iov;
//     iov.iov_base = buffer;
//     iov.iov_len = sizeof(buffer);

//     char ctrl_buf[1024]; 
//     struct msghdr msg;
//     memset(&msg, 0, sizeof(msg));
//     msg.msg_iov = &iov;
//     msg.msg_iovlen = 1;
//     msg.msg_control = ctrl_buf;
//     msg.msg_controllen = sizeof(ctrl_buf);
//     msg.msg_name = &cli_addr; 
//     msg.msg_namelen = sizeof(cli_addr);

//     // [Debug 統計] 避免刷屏，每 100 個封包印一次狀態
//     long pkt_count = 0;

//     while (running) {
//         msg.msg_controllen = sizeof(ctrl_buf); // 重要重置

//         int recv_len = recvmsg(sock_fd, &msg, 0);
//         timeType app_time = get_times(); 

//         if (recv_len > 0) {
//             pkt_count++;
            
//             // ---------------------------------------------------------
//             // [診斷點 1] 有收到控制訊息嗎？
//             // ---------------------------------------------------------
//             if (msg.msg_controllen == 0) {
//                 if (pkt_count % 100 == 0) {
//                     printf("[ERROR] 收到封包，但 msg_controllen = 0 (Onload 沒給 Metadata)\n");
//                     printf("檢查: 1. export EF_RX_TIMESTAMPING=1  2. setsockopt OPT_CMSG\n");
//                 }
//             } 
//             else {
//                 // 有收到 Metadata，檢查是不是時間戳
//                 struct cmsghdr *cmsg;
//                 bool found_any_ts = false;

//                 for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
//                     if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPING) {
//                         found_any_ts = true;
//                         struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);
                        
//                         // 抓取 HW Time (index 2)
//                         struct timespec hw_ts = ts[2];
//                         if (hw_ts.tv_sec == 0) hw_ts = ts[1]; // 退守

//                         if (hw_ts.tv_sec != 0) {
//                              // 成功！
//                              long hw_ns = (hw_ts.tv_sec * 1000000000L) + hw_ts.tv_nsec;
//                              auto app_duration = app_time.time_since_epoch();
//                              long app_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(app_duration).count();
                             
//                              // 為了 Debug，每一筆都印出來看看
//                              printf("[SUCCESS] Latency: %ld ns\n", app_ns - hw_ns);
//                         } else {
//                              printf("[WARN] 收到 SO_TIMESTAMPING 但數值是 0\n");
//                         }
//                     }
//                 }

//                 if (!found_any_ts && pkt_count % 100 == 0) {
//                     printf("[INFO] 收到 Metadata 但不是時間戳 (可能是 IP_TTL 或其他)\n");
//                 }
//             }
            
//             // 繼續原本的解析
//             ParsePackage(buffer, recv_len, app_time);

//         } else if (recv_len < 0) {
//             if (errno != EAGAIN && errno != EWOULDBLOCK) {
//                 printf("[FATAL] recvmsg error: %s\n", strerror(errno));
//             }
//         }
//     }
// }
// void UDPSession::ReceiveLoop() {
//     printf("exec ReceiveLoop Thread (Delta-Delta Mode).\n");

//     unsigned char buffer[2048];
//     struct iovec iov;
//     iov.iov_base = buffer;
//     iov.iov_len = sizeof(buffer);

//     char ctrl_buf[1024]; 
//     struct msghdr msg;
//     memset(&msg, 0, sizeof(msg));
//     msg.msg_iov = &iov;
//     msg.msg_iovlen = 1;
//     msg.msg_control = ctrl_buf;
//     msg.msg_controllen = sizeof(ctrl_buf);
//     msg.msg_name = &cli_addr; 
//     msg.msg_namelen = sizeof(cli_addr);

//     // 用來記錄「上一筆」的時間
//     long last_hw_ns = 0;
//     long last_app_ns = 0;

//     while (running) {
//         msg.msg_controllen = sizeof(ctrl_buf); 

//         int recv_len = recvmsg(sock_fd, &msg, 0);

//         // 1. 抓取 App 時間 (Monotonic)
//         struct timespec app_ts;
//         clock_gettime(CLOCK_MONOTONIC, &app_ts);
        
//         // 業務邏輯用的時間
//         timeType wall_time = get_times();

//         if (recv_len > 0) {
            
//             if (msg.msg_controllen > 0) {
//                 struct cmsghdr *cmsg;
//                 for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
//                     if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPING) {
//                         struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);
//                         struct timespec hw_ts = ts[1]; // 使用 Kernel Transformed HW Time

//                         if (hw_ts.tv_sec != 0) {
//                              long current_hw_ns = (hw_ts.tv_sec * 1000000000L) + hw_ts.tv_nsec;
//                              long current_app_ns = (app_ts.tv_sec * 1000000000L) + app_ts.tv_nsec;
                             
//                              // 只有當「上一筆」存在時，才能計算 Delta
//                              if (last_hw_ns != 0) {
//                                  // 網卡收到這兩筆的間隔
//                                  long hw_delta = current_hw_ns - last_hw_ns;
//                                  // App 收到這兩筆的間隔
//                                  long app_delta = current_app_ns - last_app_ns;
                                 
//                                  // 關鍵指標：系統處理這兩筆時，額外浪費了多少時間 (Jitter)
//                                  long overhead = app_delta - hw_delta;

//                                  // 過濾掉換頁或太久沒行情的異常值 (只看 1秒內的連續行情)
//                                  if (hw_delta > 0 && hw_delta < 1000000000) {
//                                      // 印出絕對值，方便觀察
//                                      long abs_overhead = overhead > 0 ? overhead : -overhead;
                                     
//                                      // 這裡印出的數值越小，代表 Spinning 越有效！
//                                      printf("[JITTER] %ld ns (HW_Delta: %ld | App_Delta: %ld)\n", overhead, hw_delta, app_delta);
//                                  }
//                              }

//                              // 更新「上一筆」
//                              last_hw_ns = current_hw_ns;
//                              last_app_ns = current_app_ns;
//                         } 
//                     }
//                 }
//             }
//             ParsePackage(buffer, recv_len, wall_time);
//         }
//     }
// }

// void UDPSession::ReceiveLoop() {
//     printf("exec ReceiveLoop Thread (Software Latency Mode).\n");

//     unsigned char buffer[2048];
//     struct iovec iov;
//     iov.iov_base = buffer;
//     iov.iov_len = sizeof(buffer);

//     char ctrl_buf[1024]; 
//     struct msghdr msg;
//     memset(&msg, 0, sizeof(msg));
//     msg.msg_iov = &iov;
//     msg.msg_iovlen = 1;
//     msg.msg_control = ctrl_buf;
//     msg.msg_controllen = sizeof(ctrl_buf);
//     // 這裡如果不關心是誰寄來的，可以把 name 設 NULL 簡化
//     msg.msg_name = &cli_addr; 
//     msg.msg_namelen = sizeof(cli_addr);

//     while (running) {
//         msg.msg_controllen = sizeof(ctrl_buf); // 必要重置

//         // 收取封包
//         int recv_len = recvmsg(sock_fd, &msg, 0);

//         // 1. 馬上打一個 App 時間 (現在幾點？)
//         struct timespec app_ts;
//         clock_gettime(CLOCK_REALTIME, &app_ts);

//         // 保留原本業務邏輯時間
//         timeType wall_time = get_times();

//         if (recv_len > 0) {
            
//             struct timespec *kernel_ts = NULL;
//             struct cmsghdr *cmsg;

//             // 2. 從封包裡拿出 Kernel 收到時的時間 (那時候幾點？)
//             for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
//                 if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_TIMESTAMPNS) {
//                     kernel_ts = (struct timespec *)CMSG_DATA(cmsg);
//                     break;
//                 }
//             }

//             if (kernel_ts) {
//                 // 3. 直接相減 (因為都是系統時間，不用校正，一定是正數)
//                 long diff_sec = app_ts.tv_sec - kernel_ts->tv_sec;
//                 long diff_nsec = app_ts.tv_nsec - kernel_ts->tv_nsec;
//                 long latency_ns = (diff_sec * 1000000000L) + diff_nsec;

//                 // 這就是「OS收到」到「你讀到」的時間差
//                 // 沒開 Spinning 這裡可能會是 10000 ~ 20000 ns
//                 // 有開 Spinning 這裡可能會是 1000 ~ 3000 ns
//                 printf("[LATENCY] %ld ns\n", latency_ns);
//             }

//             ParsePackage(buffer, recv_len, wall_time);

//         } else if (recv_len < 0) {
//             if (errno != EAGAIN && errno != EWOULDBLOCK) {
//                  // error log
//             }
//         }
//     }
// }

// void UDPSession::ReceiveLoop() {
//     // 1. 綁定核心 (Core 2)
//     BindThreadToCore(2); 
    
//     printf("exec ReceiveLoop Thread (Warm-up + Batch Mode).\n");

//     unsigned char buffer[2048];
//     struct iovec iov;
//     iov.iov_base = buffer;
//     iov.iov_len = sizeof(buffer);

//     char ctrl_buf[1024]; 
//     struct msghdr msg;
//     memset(&msg, 0, sizeof(msg));
//     msg.msg_iov = &iov;
//     msg.msg_iovlen = 1;
//     msg.msg_control = ctrl_buf;
//     msg.msg_controllen = sizeof(ctrl_buf);
//     msg.msg_name = &cli_addr; 
//     msg.msg_namelen = sizeof(cli_addr);

//     // 設定參數
//     const int WARMUP_COUNT = 1000; // 前 1000 筆用來熱身 (不計入統計)
//     const int TARGET_COUNT = 50;   // 熱身完後，連續抓 50 筆來算成績
    
//     long latencies[TARGET_COUNT];
//     int total_processed = 0;
//     int recorded_count = 0;

//     printf("Start Warming up for %d packets...\n", WARMUP_COUNT);

//     while (running) {
//         msg.msg_controllen = sizeof(ctrl_buf); 

//         int recv_len = recvmsg(sock_fd, &msg, 0);

//         // 1. 抓取 App 時間
//         struct timespec app_ts;
//         clock_gettime(CLOCK_REALTIME, &app_ts);

//         if (recv_len > 0) {
//             total_processed++;

//             // 如果還在熱身階段，直接跳過計算，繼續下一筆 (讓 CPU 維持高頻)
//             if (total_processed <= WARMUP_COUNT) {
//                 // ParsePackage(buffer, recv_len, get_times()); // 也可以跑邏輯熱身
//                 continue; 
//             }

//             struct timespec *kernel_ts = NULL;
//             struct cmsghdr *cmsg;

//             for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
//                 if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_TIMESTAMPNS) {
//                     kernel_ts = (struct timespec *)CMSG_DATA(cmsg);
//                     break;
//                 }
//             }

//             if (kernel_ts) {
//                 long diff_sec = app_ts.tv_sec - kernel_ts->tv_sec;
//                 long diff_nsec = app_ts.tv_nsec - kernel_ts->tv_nsec;
//                 long latency_ns = (diff_sec * 1000000000L) + diff_nsec;

//                 // 存入數據
//                 if (recorded_count < TARGET_COUNT) {
//                     latencies[recorded_count] = latency_ns;
//                     recorded_count++;
//                 }

//                 // 收滿了，輸出報告
//                 if (recorded_count >= TARGET_COUNT) {
//                     printf("\n=== 測試完成 (Warmup: %d, Record: %d) ===\n", WARMUP_COUNT, TARGET_COUNT);
                    
//                     long sum = 0;
//                     long min_lat = 999999999;
//                     long max_lat = 0;

//                     for (int i = 0; i < TARGET_COUNT; i++) {
//                         // 只印出前 10 筆和最後 5 筆，避免刷屏，或者全部印
//                         printf("[%02d] %ld ns\n", i + 1, latencies[i]);
                        
//                         sum += latencies[i];
//                         if (latencies[i] < min_lat) min_lat = latencies[i];
//                         if (latencies[i] > max_lat) max_lat = latencies[i];
//                     }

//                     printf("----------------------------------\n");
//                     printf("Min: %ld ns | Max: %ld ns | Avg: %ld ns\n", min_lat, max_lat, sum / TARGET_COUNT);
//                     printf("==================================\n");
//                     exit(0); 
//                 }
//             }
//         }
//     }
// }
// void UDPSession::ReceiveLoop() {
//     // 1. 綁定核心 (建議 Core 2)
//     BindThreadToCore(2);
    
//     printf("exec ReceiveLoop Thread (Warmup 1000 -> Record 100 -> Stats).\n");

//     unsigned char buffer[2048];
//     struct iovec iov;
//     iov.iov_base = buffer;
//     iov.iov_len = sizeof(buffer);

//     char ctrl_buf[1024]; 
//     struct msghdr msg;
//     memset(&msg, 0, sizeof(msg));
//     msg.msg_iov = &iov;
//     msg.msg_iovlen = 1;
//     msg.msg_control = ctrl_buf;
//     msg.msg_controllen = sizeof(ctrl_buf);
//     msg.msg_name = &cli_addr; 
//     msg.msg_namelen = sizeof(cli_addr);

//     // ==========================================
//     // 設定測試參數
//     // ==========================================
//     const int WARMUP_COUNT = 1000; // 預熱筆數 (不紀錄)
//     const int TARGET_COUNT = 1000;  // 正式紀錄筆數
    
//     long latencies[TARGET_COUNT];  // 存放數據的陣列
//     int total_processed = 0;       // 總共收到的封包數
//     int recorded_count = 0;        // 目前已紀錄的封包數

//     printf("Start Warming up... (Waiting for %d packets)\n", WARMUP_COUNT);

//     while (running) {
//         msg.msg_controllen = sizeof(ctrl_buf); 

//         // ---------------------------------------------------------
//         // 1. 接收 (Hot Path)
//         // ---------------------------------------------------------
//         int recv_len = recvmsg(sock_fd, &msg, 0);

//         // 2. 打卡 (App Time)
//         struct timespec app_ts;
//         clock_gettime(CLOCK_REALTIME, &app_ts);

//         if (recv_len > 0) {
//             total_processed++;

//             // [階段一] 預熱階段
//             // 如果還沒收滿 1000 筆，直接跳過後面的運算，繼續收下一筆
//             // 這樣可以讓 CPU 保持全速運轉 (Hot) 但不浪費時間做統計
//             if (total_processed <= WARMUP_COUNT) {
//                 if (total_processed == WARMUP_COUNT) {
//                     printf("Warmup Complete! Recording next %d packets...\n", TARGET_COUNT);
//                 }
//                 continue; 
//             }

//             // [階段二] 正式紀錄階段
//             struct timespec *kernel_ts = NULL;
//             struct cmsghdr *cmsg;

//             for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
//                 if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_TIMESTAMPNS) {
//                     kernel_ts = (struct timespec *)CMSG_DATA(cmsg);
//                     break;
//                 }
//             }

//             if (kernel_ts) {
//                 // 計算延遲
//                 long diff_sec = app_ts.tv_sec - kernel_ts->tv_sec;
//                 long diff_nsec = app_ts.tv_nsec - kernel_ts->tv_nsec;
//                 long latency_ns = (diff_sec * 1000000000L) + diff_nsec;

//                 // 存入陣列
//                 if (recorded_count < TARGET_COUNT) {
//                     latencies[recorded_count] = latency_ns;
//                     recorded_count++;
//                 }

//                 // [階段三] 收集完成，輸出報告並結束
//                 if (recorded_count >= TARGET_COUNT) {
//                     printf("\n=== 測試結果 (Sample Size: %d) ===\n", TARGET_COUNT);
                    
//                     long sum = 0;
//                     long min_lat = LONG_MAX;
//                     long max_lat = 0;

//                     for (int i = 0; i < TARGET_COUNT; i++) {
//                         // 印出每一筆
//                         printf("[%03d] Latency: %ld ns\n", i + 1, latencies[i]);
                        
//                         // 統計 Min/Max/Sum
//                         sum += latencies[i];
//                         if (latencies[i] < min_lat) min_lat = latencies[i];
//                         if (latencies[i] > max_lat) max_lat = latencies[i];
//                     }

//                     printf("----------------------------------\n");
//                     printf("Min: %ld ns\n", min_lat);
//                     printf("Max: %ld ns\n", max_lat);
//                     printf("Avg: %ld ns\n", sum / TARGET_COUNT);
//                     printf("==================================\n");
                    
//                     exit(0); // 程式結束
//                 }
//             }
//         } else if (recv_len < 0) {
//             // error handling...
//         }
//     }
// }
