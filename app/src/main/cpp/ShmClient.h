//
// Created by Tom on 2026/3/13.
//

#ifndef SHMIPCCLIEN_SHMCLIENT_H
#define SHMIPCCLIEN_SHMCLIENT_H

#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <sys/un.h>
#include "ShmIpcMessage.h"

#define SHM_CLIENT_HEAD_SIZE 7

#define SHM_SERVER_DEFAULT_NAME "shmIpc"

class ShmClient {
public:
    bool connectShmServer();

    int sendShmMessageAll(int socketFd, const ShmIpcMessage& message);
    int sendProtocolHeader(int sock, const char* header, int fdToSend);
};


#endif //SHMIPCCLIEN_SHMCLIENT_H
