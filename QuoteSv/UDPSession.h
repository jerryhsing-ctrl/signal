/* 
 * File:   UDPSession.h
 * Author: 0007602
 *
 * Created on 2020年7月16日, 上午 10:45
 */
#pragma once
//---------------------------------------------------------------------------
#include <functional>
#include <thread>
#include <atomic>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <list>
#include <algorithm> 
#include "timeUtil.h"
// #include "LogWriter.h"
// #include "TwseQuoteBase.h"

//---------------------------------------------------------------------------
using UdpPkgCallback = std::function<void(const char *, unsigned char*, size_t, timeType, int)>;
class UDPSession{
	public:	
		
		explicit UDPSession(const char *market, UdpPkgCallback, int idx);
		~UDPSession(void);
		
		bool Open(const char* bind_ip, const char* group_ip, int port);
		void Terminate(void) { isTerminate = true; }
		void SetFilter(std::list<unsigned short>);
		std::thread recv_thread;
		void ReceiveLoop();	
	private:                        
		struct sockaddr_in srv_addr, cli_addr;
		int 			sock_fd;			//socket
		std::atomic<bool> running;
		
		const char* 	market;
		UdpPkgCallback 	rcvPkcCallback;
		int core;
		bool 			isTerminate, isSetFilter;
		int idx;
		std::list<unsigned short> formatList;
		
		
		void CloseSocket();	
		void ParsePackage(unsigned char*, int, timeType);
        
};
//---------------------------------------------------------------------------

