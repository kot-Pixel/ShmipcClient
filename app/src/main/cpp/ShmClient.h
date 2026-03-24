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
#include "ShmMetadata.h"
#include "ShmBufferManager.h"

class ShmClient {

private:

    //protocol metadata
    ShmMetadata shmMetadata{};

    int mServerFd = -1;
    ShmMessageQueue mMessageQueue;
    std::atomic<bool> mShmReadThreadRunning{};
    std::unique_ptr<std::thread> mShmProgressThread;
    std::unique_ptr<std::thread> mShmReadThread;
    std::unique_ptr<ShmProtocolHandler> mShmProtocolHandler;

    void serverUdsReader();
    void messageProcessor();
    void clientSyncEvent(const ShmIpcMessage &message);

    ShmMetadata buildDefaultMetaData();

    ShareMemoryManager manager;

    ShmBufferManager* mgr = nullptr;

public:

    bool connectShmServer();
    void startRunReadThreadLoop();
    void stopRunReadThreadLoop();
    int sendShmMessage(const ShmIpcMessage& message) const;

    void setShmIpcMetaData(uint32_t shmSize, uint32_t sliceSize,
                           uint32_t eventQueueSize, uint32_t sliceInvalidNext, uint32_t flags);

    void sendExchangeMetadata();

    void handleExchangeMetaData(const ShmIpcMessage &message);

    void handleAckReadyRecFd();

    ShmClient() = default;

    ~ShmClient() {
        stopRunReadThreadLoop();
    }

    void handleShareMemoryReady();
    void readFromShm();
    void handleMessage(const std::vector<uint8_t>& data);
};

static ShmClient shmClient;


#endif //SHMIPCCLIEN_SHMCLIENT_H
