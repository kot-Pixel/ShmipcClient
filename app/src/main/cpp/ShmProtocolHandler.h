//
// Created by Tom on 2026/3/12.
//

#ifndef SHMIPCC_SHMPROTOCOLHANDLER_H
#define SHMIPCC_SHMPROTOCOLHANDLER_H

#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstdint>


#include "ShmLogger.h"
#include "ShmIpcMessage.h"
#include "ShmServerConfig.h"


enum class ShmProtocolType : uint8_t {
    ExchangeMetadata = 1,        // 协议协商，交换元信息
    ShareMemoryByFilePath = 2,   // 通过文件路径映射共享内存
    ShareMemoryByMemfd = 3,       // 通过 memfd 映射共享内存
    AckReadyRecvFD = 4,           // 已做好准备接收 memfd
    AckShareMemory = 5,           // 完成共享内存映射
    SyncEvent = 6,                 // 同步事件
    FallbackData = 7               // 共享内存不足时的回退数据
};

class ShmProtocolHandler {

private:
    void* shmSessionCtx = nullptr;

public:
    bool receiveProtocolHeader(int fd, uint8_t* header, std::vector<int>& received_fds);
    bool receiveProtocolPayload(int fd, char* buf, size_t len);

    explicit ShmProtocolHandler(void* extShmSessionCtx) {
        shmSessionCtx = extShmSessionCtx;
    }

    ~ShmProtocolHandler() {
        shmSessionCtx = nullptr;
    }

    int sendShmMessage(int socketFd, const ShmIpcMessage& message);
};


#endif //SHMIPCC_SHMPROTOCOLHANDLER_H
