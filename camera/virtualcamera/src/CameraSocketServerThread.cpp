/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
//#define LOG_NDEBUG 0
//#define LOG_NNDEBUG 0
#define LOG_TAG "CameraSocketServerThread: "
#include <log/log.h>

#ifdef LOG_NNDEBUG
#define ALOGVV(...) ALOGV(__VA_ARGS__)
#else
#define ALOGVV(...) ((void)0)
#endif

#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <array>
#include <atomic>

#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include "CameraSocketServerThread.h"
#include "VirtualBuffer.h"
#include "VirtualCameraFactory.h"
#include <mutex>

android::ClientVideoBuffer *android::ClientVideoBuffer::ic_instance = 0;

namespace android {

using namespace socket;
CameraSocketServerThread::CameraSocketServerThread(std::string suffix,
        std::shared_ptr<CGVideoDecoder> decoder,
        std::atomic<CameraSessionState> &state)
    : Thread(/*canCallJava*/ false), mRunning{true}, mSocketServerFd{-1},
      mVideoDecoder{decoder}, mCameraSessionState{state} {
    std::string sock_path = "/ipc/camera-socket" + suffix;
    char *k8s_env_value = getenv("K8S_ENV");
    mSocketPath = (k8s_env_value != NULL && !strcmp(k8s_env_value, "true"))
            ? "/conn/camera-socket" : sock_path.c_str();
    ALOGI("%s camera socket server path is %s", __FUNCTION__, mSocketPath.c_str());
}

CameraSocketServerThread::~CameraSocketServerThread() {
    if (mClientFd > 0) {
        shutdown(mClientFd, SHUT_RDWR);
        close(mClientFd);
        mClientFd = -1;
    }
    if (mSocketServerFd > 0) {
        close(mSocketServerFd);
        mSocketServerFd = -1;
    }
}

status_t CameraSocketServerThread::requestExitAndWait() {
    ALOGE("%s: Not implemented. Use requestExit + join instead", __FUNCTION__);
    return INVALID_OPERATION;
}

int CameraSocketServerThread::getClientFd() {
    Mutex::Autolock al(mMutex);
    return mClientFd;
}

void CameraSocketServerThread::requestExit() {
    Mutex::Autolock al(mMutex);

    ALOGV("%s: Requesting thread exit", __FUNCTION__);
    mRunning = false;
    ALOGV("%s: Request exit complete.", __FUNCTION__);
}

status_t CameraSocketServerThread::readyToRun() {
    Mutex::Autolock al(mMutex);

    return OK;
}

void CameraSocketServerThread::clearBuffer() {
    ALOGVV(LOG_TAG " %s Enter", __FUNCTION__);
    mSocketBuffer.fill(0);
    ClientVideoBuffer *handle = ClientVideoBuffer::getClientInstance();
    char *fbuffer = (char *)handle->clientBuf[handle->clientRevCount % 1].buffer;

    if (gIsInFrameI420) {
        // TODO: Use width and height for current resolution
        clearBuffer(fbuffer, 640, 480);
    }
    ALOGVV(LOG_TAG " %s: Exit", __FUNCTION__);
}

void CameraSocketServerThread::clearBuffer(char *buffer, int width, int height) {
    ALOGVV(LOG_TAG " %s Enter", __FUNCTION__);
    char *uv_offset = buffer + width * height;
    memset(buffer, 0x10, (width * height));
    memset(uv_offset, 0x80, (width * height) / 2);
    ALOGVV(LOG_TAG " %s: Exit", __FUNCTION__);
}

bool CameraSocketServerThread::threadLoop() {
    mSocketServerFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (mSocketServerFd < 0) {
        ALOGE("%s:%d Fail to construct camera socket with error: %s", __FUNCTION__, __LINE__,
              strerror(errno));
        return false;
    }

    struct sockaddr_un addr_un;
    memset(&addr_un, 0, sizeof(addr_un));
    addr_un.sun_family = AF_UNIX;
    strncpy(&addr_un.sun_path[0], mSocketPath.c_str(), strlen(mSocketPath.c_str()));

    int ret = 0;
    if ((access(mSocketPath.c_str(), F_OK)) != -1) {
        ALOGI(" %s camera socket server file is %s", __FUNCTION__, mSocketPath.c_str());
        ret = unlink(mSocketPath.c_str());
        if (ret < 0) {
            ALOGE(LOG_TAG " %s Failed to unlink %s address %d, %s", __FUNCTION__,
                  mSocketPath.c_str(), ret, strerror(errno));
            return false;
        }
    } else {
        ALOGV(LOG_TAG " %s camera socket server file %s will created. ", __FUNCTION__,
              mSocketPath.c_str());
    }

    ret = ::bind(mSocketServerFd, (struct sockaddr *)&addr_un,
                 sizeof(sa_family_t) + strlen(mSocketPath.c_str()) + 1);
    if (ret < 0) {
        ALOGE(LOG_TAG " %s Failed to bind %s address %d, %s", __FUNCTION__, mSocketPath.c_str(),
              ret, strerror(errno));
        return false;
    }

    struct stat st;
    __mode_t mod = S_IRWXU | S_IRWXG | S_IRWXO;
    if (fstat(mSocketServerFd, &st) == 0) {
        mod |= st.st_mode;
    }
    chmod(mSocketPath.c_str(), mod);
    stat(mSocketPath.c_str(), &st);

    ret = listen(mSocketServerFd, 5);
    if (ret < 0) {
        ALOGE("%s Failed to listen on %s", __FUNCTION__, mSocketPath.c_str());
        return false;
    }

    while (mRunning) {
        ALOGI(LOG_TAG " %s: Wait for camera client to connect. . .", __FUNCTION__);

        socklen_t alen = sizeof(struct sockaddr_un);

        int new_client_fd = ::accept(mSocketServerFd, (struct sockaddr *)&addr_un, &alen);
        ALOGI(LOG_TAG " %s: Accepted client: [%d]", __FUNCTION__, new_client_fd);
        if (new_client_fd < 0) {
            ALOGE(LOG_TAG " %s: Fail to accept client. Error: [%s]", __FUNCTION__, strerror(errno));
            continue;
        }
        mClientFd = new_client_fd;

        ClientVideoBuffer *handle = ClientVideoBuffer::getClientInstance();
        char *fbuffer = (char *)handle->clientBuf[handle->clientRevCount % 1].buffer;

        clearBuffer(fbuffer, 640, 480);

        struct pollfd fd;
        int event;

        fd.fd = mClientFd;  // your socket handler
        fd.events = POLLIN | POLLHUP;

        while (true) {
            // check if there are any events on fd.
            int ret = poll(&fd, 1, 3000);  // 1 second for timeout

            event = fd.revents;  // returned events

            if (event & POLLHUP) {
                // connnection disconnected => socket is closed at the other end => close the
                // socket.
                ALOGE(LOG_TAG " %s: POLLHUP: Close camera socket connection", __FUNCTION__);
                shutdown(mClientFd, SHUT_RDWR);
                close(mClientFd);
                mClientFd = -1;
                clearBuffer(fbuffer, 640, 480);
                break;
            } else if (event & POLLIN) {  // preview / record
                // data is available in socket => read data
                if (gIsInFrameI420) {
                    ssize_t size = 0;

                    if ((size = recv(mClientFd, (char *)fbuffer, 460800, MSG_WAITALL)) > 0) {
                        handle->clientRevCount++;
                        ALOGVV(LOG_TAG
                               "[I420] %s: Pocket rev %d and "
                               "size %zd",
                               __FUNCTION__, handle->clientRevCount, size);
                    }
                } else if (gIsInFrameH264) {  // default H264
                    size_t recv_frame_size = 0;
                    ssize_t size = 0;
                    if ((size = recv(mClientFd, (char *)&recv_frame_size, sizeof(size_t),
                                     MSG_WAITALL)) > 0) {
                        ALOGVV("[H264] Received Header %zd bytes. Payload size: %zu", size,
                              recv_frame_size);
                        if (recv_frame_size > mSocketBuffer.size()) {
                            // maximum size of a H264 packet in any aggregation packet is 65535
                            // bytes. Source: https://tools.ietf.org/html/rfc6184#page-13
                            ALOGE(
                                "%s Fatal: Unusual H264 packet size detected: %zu! Max is %zu, ...",
                                __func__, recv_frame_size, mSocketBuffer.size());
                            continue;
                        }
                        // recv frame
                        if ((size = recv(mClientFd, (char *)mSocketBuffer.data(), recv_frame_size,
                                         MSG_WAITALL)) > 0) {
                            mSocketBufferSize = recv_frame_size;
                            ALOGVV("%s [H264] Camera session state: %s", __func__,
                                  kCameraSessionStateNames.at(mCameraSessionState).c_str());
                            switch (mCameraSessionState) {
                                case CameraSessionState::kCameraOpened:
                                    mCameraSessionState = CameraSessionState::kDecodingStarted;
                                    ALOGVV("%s [H264] Decoding started now.", __func__);
                                case CameraSessionState::kDecodingStarted:
                                    mVideoDecoder->decode(mSocketBuffer.data(), mSocketBufferSize);
                                    handle->clientRevCount++;
                                    ALOGVV("%s [H264] Received Payload #%d %zd/%zu bytes", __func__,
                                          handle->clientRevCount, size, recv_frame_size);
                                    break;
                                case CameraSessionState::kCameraClosed:
                                    mVideoDecoder->flush_decoder();
                                    mVideoDecoder->destroy();
                                    mCameraSessionState = CameraSessionState::kDecodingStopped;
                                    ALOGVV("%s [H264] Decoding stopped now.", __func__);
                                    break;
                                case CameraSessionState::kDecodingStopped:
                                    ALOGVV("%s [H264] Decoding is already stopped, skip the packets",
                                          __func__);
                                default:
                                    ALOGE("%s [H264] Invalid Camera session state!", __func__);
                                    break;
                            }
                        }
                    }
                } else {
                    ALOGE("%s: only H264, I420 input frames supported", __FUNCTION__);
                }
            } else {
                //    ALOGE("%s: continue polling..", __FUNCTION__);
            }
        }
    }
    ALOGE(" %s: Quit CameraSocketServerThread... %s(%d)", __FUNCTION__, mSocketPath.c_str(),
          mClientFd);
    shutdown(mClientFd, SHUT_RDWR);
    close(mClientFd);
    mClientFd = -1;
    close(mSocketServerFd);
    mSocketServerFd = -1;
    return true;
}

}  // namespace android
