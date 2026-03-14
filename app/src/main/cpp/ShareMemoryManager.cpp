//
// Created by kotlinx on 2026/3/14.
//


#include "ShareMemoryManager.h"


int ShareMemoryManager::createShareMemory(int size) {
    int fd = syscall(SYS_memfd_create, SHARE_MEMORY_NAME, MFD_CLOEXEC);
    if (fd < 0) {
        LOGI("memfd_create failed");
        return -1;
    }

    if (ftruncate(fd, size) < 0) {
        LOGI("ftruncate failed");
        close(fd);
        return -1;
    }

    void* addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        LOGE("mmap failed");
        close(fd);
        return -1;
    }

    memset(addr, 0, size);
    return fd;
}
