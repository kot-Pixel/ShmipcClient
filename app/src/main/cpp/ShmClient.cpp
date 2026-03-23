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

    int ret = connect(mServerFd, (struct sockaddr*)&addr, len);

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

int ShmClient::sendShmMessage(const ShmIpcMessage& message) const {
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
        ret = mShmProtocolHandler->receiveProtocolHeader(mServerFd, udsProtocolHeader, received_fds);
        if (ret) {
            ShmIpcMessage msg;
            msg.header = ShmIpcMessageHeader::deserialize(udsProtocolHeader);
            uint32_t payloadLength = msg.header.length - SHM_SERVER_PROTOCOL_HEAD_SIZE;
            std::vector<char> payload(payloadLength);
            if (payloadLength > 0) {
                if (!mShmProtocolHandler->receiveProtocolPayload(mServerFd, payload.data(), payloadLength)) {
                    LOGE("unix domain socket payload read failure");
                    break;
                }
            }
            msg.payload = std::move(payload);
            msg.fds = std::move(received_fds);
            mMessageQueue.push(std::move(msg));
        }else {
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
                LOGD("ex changed meta data.........");
                break;
            }

            case ShmProtocolType::ShareMemoryByMemfd: {
                break;
            }

            case ShmProtocolType::SyncEvent: {
                clientSyncEvent(msg);
                break;
            }

            case ShmProtocolType::AckShareMemory: {

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

void ShmClient::ackShareMemory(const ShmIpcMessage &message) {
    LOGD("ackShareMemory");
}
