#include <jni.h>
#include <string>
#include "ShmClient.h"

extern "C" JNIEXPORT jstring JNICALL
Java_com_kotlinx_shmipcclien_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";

    ShmClient shmClient;
    shmClient.connectShmServer();

    return env->NewStringUTF(hello.c_str());
}