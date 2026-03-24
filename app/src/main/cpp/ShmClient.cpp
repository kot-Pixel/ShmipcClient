//
// Created by Tom on 2026/3/13.
//

#include "ShmClient.h"


bool ShmClient::connectShmServer() {

    LOGD("start connect shm server");

    mServerFd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (mServerFd == -1) {
        LOGE("server fd is not invalid");
        return false;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;

    addr.sun_path[0] = '\0';
    strcpy(addr.sun_path + 1, SHM_SERVER_DEFAULT_NAME);

    int len = offsetof(struct sockaddr_un, sun_path) + 1 + strlen(SHM_SERVER_DEFAULT_NAME);

    int ret = connect(mServerFd, (struct sockaddr *) &addr, len);

    if (ret != 0) {
        LOGD("connect shm server failure");
        close(mServerFd);
        mServerFd = -1;
        return false;
    }

    LOGD("connect shm server success");
    startRunReadThreadLoop();
    LOGD("start uds socket read thread loop");

    return true;
}

int ShmClient::sendShmMessage(const ShmIpcMessage &message) const {
    if (mServerFd == -1) {
        return -1;
    }
    return mShmProtocolHandler->sendShmMessage(mServerFd, message);
}

void ShmClient::startRunReadThreadLoop() {
    mShmReadThreadRunning = true;
    mShmProgressThread.reset(new std::thread(&ShmClient::messageProcessor, this));
    mShmReadThread.reset(new std::thread(&ShmClient::serverUdsReader, this));
}

void ShmClient::stopRunReadThreadLoop() {
    mShmReadThreadRunning = false;
    if (mShmReadThread && mShmReadThread->joinable()) {
        mShmReadThread->join();
    }
    mMessageQueue.stop();
    if (mShmProgressThread && mShmProgressThread->joinable()) {
        mShmProgressThread->join();
    }
}

void ShmClient::serverUdsReader() {
    LOGI("ShmServer Session Thread Start");

    uint8_t udsProtocolHeader[SHM_SERVER_PROTOCOL_HEAD_SIZE];
    std::vector<int> received_fds;
    int ret;

    while (mShmReadThreadRunning) {
        ret = mShmProtocolHandler->receiveProtocolHeader(mServerFd, udsProtocolHeader,
                                                         received_fds);
        if (ret) {
            ShmIpcMessage msg;
            msg.header = ShmIpcMessageHeader::deserialize(udsProtocolHeader);
            uint32_t payloadLength = msg.header.length - SHM_SERVER_PROTOCOL_HEAD_SIZE;
            std::vector<char> payload(payloadLength);
            if (payloadLength > 0) {
                if (!mShmProtocolHandler->receiveProtocolPayload(mServerFd, payload.data(),
                                                                 payloadLength)) {
                    LOGE("unix domain socket payload read failure");
                    break;
                }
            }
            msg.payload = std::move(payload);
            msg.fds = std::move(received_fds);
            mMessageQueue.push(std::move(msg));
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            LOGE("unix domain socket header read failure");
            break;
        }
    }
    LOGD("mServerFd close");
    close(mServerFd);

    mShmReadThreadRunning = false;

    mMessageQueue.stop();
    if (mShmProgressThread && mShmProgressThread->joinable()) {
        mShmProgressThread->join();
    }
    LOGI("ShmServer Session Thread Stop");
}

void ShmClient::messageProcessor() {
    LOGI("client message processor thread start");

    ShmIpcMessage msg;
    while (mMessageQueue.pop(msg)) {
        LOGI("Processing message, payload size: %zu, fds count: %zu",
             msg.payload.size(), msg.fds.size());
        auto messageType = static_cast<ShmProtocolType>(msg.header.type);
        switch (messageType) {
            case ShmProtocolType::ExchangeMetadata: {
                handleExchangeMetaData(msg);
                break;
            }

            case ShmProtocolType::AckReadyRecvFD: {
                handleAckReadyRecFd();
                break;
            }

            case ShmProtocolType::ShareMemoryReady: {
                handleShareMemoryReady();
                break;
            }

            case ShmProtocolType::SyncEvent: {
                clientSyncEvent(msg);
                break;
            }
            default:
                break;
        }
    }

    LOGI("client message processor thread stop");
}

void ShmClient::clientSyncEvent(const ShmIpcMessage &message) {
    LOGD("clientSyncEvent");
}

void ShmClient::setShmIpcMetaData(uint32_t shmSize, uint32_t sliceSize, uint32_t eventQueueSize,
                                  uint32_t sliceInvalidNext, uint32_t flags) {
    ShmMetadata meta{};
    meta.magic = SHM_MAGIC;
    meta.version = SHM_VERSION;

    meta.shmSize = shmSize;
    meta.sliceSize = sliceSize;
    meta.eventQueueSize = eventQueueSize;
    meta.sliceInvalidNext = sliceInvalidNext;
    meta.flags = flags;

    shmMetadata = meta;
}

ShmMetadata ShmClient::buildDefaultMetaData() {
    ShmMetadata meta{};
    meta.magic = SHM_MAGIC;
    meta.version = SHM_VERSION;

    meta.shmSize = SHM_META_DATA_DEFAULT_SHARE_MEMORY_SIZE;
    meta.sliceSize = SHM_META_DATA_SLICE_SIZE;
    meta.eventQueueSize = SHM_META_DATA_EVENT_QUEUE_SIZE;
    meta.sliceInvalidNext = SHM_META_DATA_INVALID_SLICE_INDEX;
    meta.flags = 0;

    return meta;
}

/**
 * send ExChange Meta data..
 */
void ShmClient::sendExchangeMetadata() {
    ShmMetadata metadata = shmMetadata;
    bool customMetaData = metaDataIsValid(metadata);

    if (!customMetaData) {
        metadata = buildDefaultMetaData();
        LOGI("use default meta data");
    }

    customMetaData = metaDataIsValid(metadata);
    if (customMetaData) {
        ShmIpcMessage msg;
        msg.payload.resize(sizeof(ShmMetadata));
        memcpy(msg.payload.data(), &metadata, sizeof(ShmMetadata));
        auto type = static_cast<uint8_t>(ShmProtocolType::ExchangeMetadata);
        auto length = SHM_SERVER_PROTOCOL_HEAD_SIZE + msg.payload.size();
        msg.header = ShmIpcMessageHeader(type, length, msg.fds.size());
        int result = sendShmMessage(msg);
        LOGD("exchange meta data event %d", result);
    } else {
        LOGE("meta data is valid");
    }
}

/**
 * handle ExChange Meta data, and send share memory mem fd
 * @param message
 */
void ShmClient::handleExchangeMetaData(const ShmIpcMessage &message) {
    ShmMetadata meta{};
    ShmIpcMessage msg;

    if (message.payload.size() < sizeof(ShmMetadata)) {
        LOGE("payload too small");
        return;
    }
    memcpy(&meta, message.payload.data(), sizeof(ShmMetadata));
    if (!metaDataIsValid(meta)) {
        LOGE("invalid metadata");
        return;
    }
    shmMetadata = meta;

    auto type = static_cast<uint8_t>(ShmProtocolType::ShareMemoryByMemfd);
    auto length = SHM_SERVER_PROTOCOL_HEAD_SIZE;
    msg.header = ShmIpcMessageHeader(type, length, msg.fds.size());

    int result = sendShmMessage(msg);
    LOGD("share memory by mem fd %d", result);
}

/**
 * server ready to receive shm
 */
void ShmClient::handleAckReadyRecFd() {

    LOGD("handleAckReadyRecFd invoke");

    bool mShmResult = manager.createShareMemory(shmMetadata.shmSize);
    if (!mShmResult) {
        LOGE("createShareMemory failure");
        return;
    } else {
        LOGD("shmFd fd=%d\n", manager.shareMemoryFd);
    }

    ShmIpcMessage msg;
    msg.fds.push_back(manager.shareMemoryFd);
    auto type = static_cast<uint8_t>(ShmProtocolType::AckShareMemory);
    auto length = SHM_SERVER_PROTOCOL_HEAD_SIZE;
    msg.header = ShmIpcMessageHeader(type, length, msg.fds.size());

    int result = sendShmMessage(msg);

    LOGD("ack share mem fd %d", result);
}

void ShmClient::handleShareMemoryReady() {
    if (manager.shareMemoryFd != -1 && manager.shareMemoryAddr != MAP_FAILED) {
        if (mgr == nullptr) {
            mgr = init_shm_buffer_manager(manager.shareMemoryAddr, shmMetadata.shmSize);
        }
        LOGD("ShmBufferManager: io_queue capacity=%u, slice_count=%u\n",
             mgr->io_queue.capacity,
             mgr->buffer_list.slice_count);
    }
}

