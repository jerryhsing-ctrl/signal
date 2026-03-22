#pragma once 
#include "Format6.h"
#include "SPSC.h"
#include <string>
#include <cstdint> // 用於 uint8_t

#define Range 10000
using format6Type = Format6;
// using queueType = SPSCQueue<format6Type, 1000>;
using queueType = SPSCQueue<format6Type, 10000000>;

using seqnoType = string;

enum class MatchType {
    None,
    StrongSingle,
    StrongGroup,
    Both
};

enum class Reply : uint32_t {
    Upload_Success,
    Upload_Repeat,
    Cancel_Success,
    Cancel_Seqno_Nonexist_In_StrategySv,
    Cancel_Seqno_Nonexist_In_Mgr,
    Cancel_Mgr_Nonexist_In_Worker,
    Cancel_Already_Executed,
    Cancel_Already_Canceled,
    Cancel_Already_Close,
    Query_Reply,
    Report_Reply,
    Heartbeat
    
};

struct SpscBack {
    int clientFd;
    seqnoType seqno;
    Reply reply;
    int64_t time;
    format6Type *f6;

    // 預設建構子 (SPSCQueue buffer 初始化需要)
    // SpscBack() = default;

    // // 完整參數建構子
    // SpscBack(int fd, seqnoType seq, Reply r, int64_t t, format6Type* f)
    //     : clientFd(fd), seqno(seq), reply(r), time(t), f6(f) {}
};