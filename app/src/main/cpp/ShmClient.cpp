//
// Created by Tom on 2026/3/13.
//


#include <fcntl.h>
#include "ShmClient.h"
#include "ShmLogger.h"
#include "ShareMemoryManager.h"

bool ShmClient::connectShmServer() {

    LOGD("start connect shm server");

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;

    addr.sun_path[0] = '\0';
    strcpy(addr.sun_path + 1, SHM_SERVER_DEFAULT_NAME);

    int len = offsetof(struct sockaddr_un, sun_path) + 1 + strlen(SHM_SERVER_DEFAULT_NAME);

    int ret = connect(fd, (struct sockaddr*)&addr, len);

    if (ret != 0) {
        LOGD("connect shm server failure");
        close(fd);
        return -1;
    }

    LOGD("connect shm server success");


    ShareMemoryManager manager;

    int shmFd = manager.createShareMemory(16 * 1024);

    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd < 0) {
        LOGE("eventfd create failure");
    } else {
        LOGD("eventfd create success");
    }

    if (shmFd < 0) {
        LOGD("shmFd failed");
    } else {
        LOGD("shmFd fd=%d\n", shmFd);
    }

    ShmIpcMessage msg, msg2;
    msg.fds.push_back(efd);
    msg2.fds.push_back(shmFd);
    msg.header = ShmIpcMessageHeader(1, 7, msg.fds.size());
    msg2.header = ShmIpcMessageHeader(3, 7, msg2.fds.size());

    sendShmMessageAll(fd, msg);
    sendShmMessageAll(fd, msg2);

    return fd;
}

int ShmClient::sendShmMessageAll(int socketFd, const ShmIpcMessage& message) {
    struct msghdr msg{};
    struct iovec iov[2];

    auto headerBytes = message.header.serialize();

    iov[0].iov_base = (void*)headerBytes.data();
    iov[0].iov_len  = headerBytes.size();

    iov[1].iov_base = (void*)message.payload.data();
    iov[1].iov_len  = message.payload.size();

    msg.msg_iov = iov;
    msg.msg_iovlen = message.payload.empty() ? 1 : 2;

    char cmsgbuf[CMSG_SPACE(sizeof(int) * message.fds.size())];

    if (!message.fds.empty())
    {

        LOGD("send fds start");

        msg.msg_control = cmsgbuf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * message.fds.size());

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type  = SCM_RIGHTS;
        cmsg->cmsg_len   = CMSG_LEN(sizeof(int) * message.fds.size());

        memcpy(CMSG_DATA(cmsg), message.fds.data(),
               sizeof(int) * message.fds.size());

        LOGD("send fds end");
    }

    ssize_t n = sendmsg(socketFd, &msg, 0);

    if (n < 0)
    {
        perror("sendmsg");
        return -1;
    }

    return 0;
}

int ShmClient::sendProtocolHeader(int sock, const char* header, int fdToSend) {
    struct msghdr msg{};
    struct iovec iov;

    // 1. iovec 指向 header
    iov.iov_base = (void*)header;
    iov.iov_len  = SHM_CLIENT_HEAD_SIZE;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    // 2. 辅助数据缓冲区，用于发送 fd
    char cmsgbuf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    cmsg->cmsg_len   = CMSG_LEN(sizeof(int));

    memcpy(CMSG_DATA(cmsg), &fdToSend, sizeof(int));

    // 3. 发送
    ssize_t n = sendmsg(sock, &msg, 0);
    bool result = (n == SHM_CLIENT_HEAD_SIZE);
    LOGD("sendmsg result is: %d", result);

    return result;
}