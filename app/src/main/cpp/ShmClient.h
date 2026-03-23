//
// Created by Tom on 2026/3/13.
//

#ifndef SHMIPCCLIEN_SHMCLIENT_H
#define SHMIPCCLIEN_SHMCLIENT_H

#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <sys/un.h>
#include <thread>
#include <fcntl.h>

#include "ShmLogger.h"
#include "ShareMemoryManager.h"
#include "ShmIpcMessage.h"
#include "ShmMessageQueue.h"
#include "ShmProtocolHandler.h"

class ShmClient {

private:
    int mServerFd = -1;
    ShmMessageQueue mMessageQueue;
    std::atomic<bool> mShmReadThreadRunning{};
    std::unique_ptr<std::thread> mShmProgressThread;
    std::unique_ptr<std::thread> mShmReadThread;
    std::unique_ptr<ShmProtocolHandler> mShmProtocolHandler;

    void serverUdsReader();
    void messageProcessor();
    void clientSyncEvent(const ShmIpcMessage &message);
    void ackShareMemory(const ShmIpcMessage &message);
public:

    bool connectShmServer();
    void startRunReadThreadLoop();
    void stopRunReadThreadLoop();
    int sendShmMessage(const ShmIpcMessage& message) const;
    ShmClient() = default;

    ~ShmClient() {
        stopRunReadThreadLoop();
    }
};

static ShmClient shmClient;


#endif //SHMIPCCLIEN_SHMCLIENT_H
