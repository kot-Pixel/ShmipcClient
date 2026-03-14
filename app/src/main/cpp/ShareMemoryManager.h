//
// Created by kotlinx on 2026/3/14.
//

#ifndef SHMIPCCLIEN_SHAREMEMORYMANAGER_H
#define SHMIPCCLIEN_SHAREMEMORYMANAGER_H

#include <sys/mman.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include "ShmLogger.h"
#include <cstring>

#define SHARE_MEMORY_NAME "shareMemory"

class ShareMemoryManager {
public:
    int createShareMemory(int size);
};


#endif //SHMIPCCLIEN_SHAREMEMORYMANAGER_H
