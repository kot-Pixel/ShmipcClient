#include <jni.h>
#include <string>
#include "ShmClient.h"
#include "ShmLogger.h"
#include "ShareMemoryManager.h"

extern "C" JNIEXPORT jstring JNICALL
Java_com_kotlinx_shmipcclien_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";

    bool result = shmClient.connectShmServer();
    LOGD("connect shm server result is: %d" , result);

    if (result) {
        ShareMemoryManager manager;
        int shmFd = manager.createShareMemory(16 * 1024 * 1024);

        if (shmFd < 0) {
            LOGD("shmFd failed");
        } else {
            LOGD("shmFd fd=%d\n", shmFd);
        }

        ShmIpcMessage msg;
        msg.fds.push_back(shmFd);
        auto type = static_cast<uint8_t>(ShmProtocolType::ShareMemoryByMemfd);
        auto length = SHM_SERVER_PROTOCOL_HEAD_SIZE;
        msg.header = ShmIpcMessageHeader(type, length, msg.fds.size());

        result = shmClient.sendShmMessage(msg);
        LOGD("sync data event %d", result);
    }

    return env->NewStringUTF(hello.c_str());
}