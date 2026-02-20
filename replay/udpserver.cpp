#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "IniReader.h"

// #define MULTICAST_IP  "239.255.0.1"
// #define MULTICAST_PORT 10000
// #define MYIP "10.175.1.33"

#define MULTICAST_IP  "239.255.0.1"
#define MULTICAST_PORT 10000
#define MYIP "10.175.1.33"

using namespace std;
struct StrcutPackHeader {
    unsigned char ESCCODE_;         //ASCII 27
    unsigned char PacketLen[2];     //訊息長度9(04) PACK BCD 
    unsigned char PacketType;	    //業務別9(02) (01:上市 02:上櫃) PACK BCD 
    unsigned char PacketCode;	    //傳輸格式代碼9(02) PACK BCD 
    unsigned char PacketVersion;	//傳輸格式版別9(02) PACK BCD 
    unsigned char PacketSeqNo[4];	//傳輸序號 9(08)  PACK BCD 
};

std::string PackBCD2Str(unsigned char* src, int count) {
    //1111 11
        //1 2 4 8
    std::string rtnstr;
    
    //實作這段將BCD Char轉成數字字串
    for (int i = 0; i < count; i++) {
        int a, b;
        a = src[i];
        a = a >> 4;
        rtnstr += std::to_string(a);
        b = src[i];
        b = b & 15;
        rtnstr += std::to_string(b);
    }
    return rtnstr;
}
//---------------------------------------------------------------------------------------------------------------------------------------------
int PackBCD2Int(unsigned char* src, int count) { 
    std::string tmp = PackBCD2Str(src, count);
    int tmpD = atoi(tmp.c_str());
    return tmpD;
}


int main(int argc, char* argv[]) {
    IniReader reader("../exec/cfg/HWStratSv.cfg");
    // cout << "hi\n";
    string my_ip = reader.Read("HWQ", "MYIP");;
    string mcast_ip = reader.Read("HWQ", "IP");
    int mcast_port = stoi(reader.Read("HWQ", "PORT"));

    std::cout << "開始從檔案發送 UDP 多播資料到 "
              << mcast_ip << ":" << mcast_port
              << " (介面 " << my_ip << ")\n";
    // cout << mcast_ip << "\n";
    // cout << mcast_port << "\n";
    if (argc != 2) {
        std::cerr << "用法: " << argv[0] << " <輸入檔案名稱>\n";
        return 1;
    }
    std::string filename = std::string(argv[1]);
    // std::string MYIP = "10.101.1.103";   // 與 client argv[1] 一致

    // 2. 建立 UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    // 3. 指定發送用的本機網卡（必須與 client MyIP 相同）
    in_addr local_if{};
    local_if.s_addr = inet_addr(my_ip.c_str());
    if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, &local_if, sizeof(local_if)) < 0) {
        perror("setsockopt(IP_MULTICAST_IF)");
        close(sockfd);
        return 1;
    }

    // 4. 開啟 loopback（同機測試必須開）
    int loop = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
        perror("setsockopt(IP_MULTICAST_LOOP)");
        close(sockfd);
        return 1;
    }

    // 5. 設定多播目標地址
    sockaddr_in mcast_addr{};
    memset(&mcast_addr, 0, sizeof(mcast_addr));
    mcast_addr.sin_family = AF_INET;
    mcast_addr.sin_port = htons(mcast_port);
    inet_pton(AF_INET, mcast_ip.c_str(), &mcast_addr.sin_addr);

    

    // 6. 逐行讀取檔案並發送
    /*
    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty()) continue;

        ssize_t sent = sendto(sockfd, line.c_str(), line.size(), 0,
                              reinterpret_cast<sockaddr*>(&mcast_addr), sizeof(mcast_addr));
        if (sent < 0) {
            perror("sendto");
        } else {
            std::cout <<  "sent len : " << line.size() << "\n";
        }

        usleep(2000); // 控制發送速度，2ms 可視需求調整
    }
    */	
	const size_t BUF_SIZE = 512;
    char* bodyBuffer = new char[BUF_SIZE];   // 暫存每行資料
    size_t pos = 0;                          // 目前填入的位置
	int cnt=0;
    char ch;
	std::ifstream infile(filename, std::ios::binary);  // 二進位方式讀取
	int times = 0;
	while (true)
	{
	    if (!infile.is_open()) {
			std::cerr << "無法開啟檔案: " << filename << "\n";
			return 1;
		}
		
		while (infile.get(ch)) {                 // 一次讀取一個字元
			// if (ch == '\n' || ch == 0x0A) {      // 遇到換行符號
			// 	bodyBuffer[pos++] = ch;
			// 	//bodyBuffer[pos] = '\0';          // 結束字串
			// 	//std::cout << "讀到一筆: " << bodyBuffer << std::endl;
				
			// 	ssize_t sent = sendto(sockfd, bodyBuffer, pos, 0, reinterpret_cast<sockaddr*>(&mcast_addr), sizeof(mcast_addr));
			// 	std::cout << "\nsent len: " << sent << "\n";
            //     StrcutPackHeader* header = reinterpret_cast<StrcutPackHeader*>(bodyBuffer);
	        //     std::cout << " PacketLen " << PackBCD2Str(header->PacketLen, 2) << " PacketCode " << PackBCD2Str(&(header->PacketCode), 1) << '\n';

            //     times++;
            //     if (times == 2) {
            //         std::cout << "sent len: " << times << "\n";
            //         return 0;
            //     }

            //     // std::cout << "send 1 data\n";
                
                
            //     //if (cnt++>10) break;
			// 	// 清空，準備讀下一筆
			// 	usleep(1000);
			// 	pos = 0;
			// 	std::memset(bodyBuffer, 0, BUF_SIZE);
			// } else {
			// 	if (pos < BUF_SIZE - 1) {        // 預防溢位
			// 		bodyBuffer[pos++] = ch;
			// 	} else {
			// 		std::cerr << "警告: 緩衝區已滿，該行資料過長\n";
			// 		// 可以選擇擴充緩衝區或丟棄多餘資料
			// 	}
			// }		
            // if (pos == BUF_SIZE) {
            //     std::cout << "out of range\n";
            //     return 1;
            // }
            bodyBuffer[pos++] = ch;
            if (pos >= 2 && bodyBuffer[pos - 2] == 0x0D && bodyBuffer[pos - 1] == 0x0A) {
                ssize_t sent = sendto(sockfd, bodyBuffer, pos, 0, reinterpret_cast<sockaddr*>(&mcast_addr), sizeof(mcast_addr));
                if (sent < 32 || sent > 131 || (sent - 32) % 9 != 0) {
                    std::cout << "len invalid\n";
                    return 1;
                }
				// std::cout << "\nsent len: " << sent << "\n";
                StrcutPackHeader* header = reinterpret_cast<StrcutPackHeader*>(bodyBuffer);
	            // std::cout << " PacketLen " << PackBCD2Str(header->PacketLen, 2) << " PacketCode " << PackBCD2Str(&(header->PacketCode), 1) << '\n';

                times++;
                // if (times == 2) {
                //     std::cout << "sent len: " << times << "\n";
                //     return 0;
                // }

                // std::cout << "send 1 data\n";
                
                
                //if (cnt++>10) break;
				// 清空，準備讀下一筆
				usleep(1000);
                // sleep(1);
				pos = 0;
				std::memset(bodyBuffer, 0, BUF_SIZE);
            }
        }
        std::cout << "finish\n";
        break;
		infile.clear();                 // 清掉 EOF
		infile.seekg(0, std::ios::beg); // 回到檔頭		
		std::cout << "-------------------------------------------------------------------------------" << std::endl;
	}
	infile.close();
    delete[] bodyBuffer;
    
    return 0;



    close(sockfd);
    infile.close();
    return 0;
}