#ifndef HW_EMULATOR_CAMERA_VIRTUALD_CAMERA_FACTORY_H_K
#define HW_EMULATOR_CAMERA_VIRTUALD_CAMERA_FACTORY_H_K

#include <mutex>

#define MAX_CLIENT_BUF 8
namespace android {

extern bool gIsInFrameI420;
extern bool gIsInFrameH264;
extern bool gUseVaapi;

enum class VideoBufferType {
    kI420,
    kARGB,
};

struct Resolution {
    int width = 640;
    int height = 480;
};
/// Video buffer and its information
struct VideoBuffer {
    /// Video buffer
    uint8_t* buffer;
    /// Resolution for the Video buffer
    Resolution resolution;
    // Buffer type
    VideoBufferType type;
    ~VideoBuffer() {}

    void reset() {
        std::fill(buffer, buffer + resolution.width * resolution.height, 0x10);
        uint8_t* uv_offset = buffer + resolution.width * resolution.height;
        std::fill(uv_offset, uv_offset + (resolution.width * resolution.height) / 2, 0x80);
        decoded = false;
    }
    bool decoded = false;
};

class ClientVideoBuffer {
public:
    static ClientVideoBuffer* ic_instance;

    struct VideoBuffer clientBuf[1];
    unsigned int clientRevCount = 0;
    unsigned int clientUsedCount = 0;

    size_t receivedFrameNo = 0;
    size_t decodedFrameNo = 0;

    static ClientVideoBuffer* getClientInstance() {
        if (ic_instance == NULL) {
            ic_instance = new ClientVideoBuffer();
        }
        return ic_instance;
    }

    ClientVideoBuffer() {
        for (int i = 0; i < 1; i++) {
            clientBuf[i].buffer =
                new uint8_t[clientBuf[i].resolution.width * clientBuf[i].resolution.height * 3 / 2];
        }
        clientRevCount = 0;
        clientUsedCount = 0;
    }

    ~ClientVideoBuffer() {
        for (int i = 0; i < 1; i++) {
            delete[] clientBuf[i].buffer;
        }
    }

    void reset() {
        for (int i = 0; i < 1; i++) {
            clientBuf[i].reset();
        }
        clientRevCount = clientUsedCount = 0;
        receivedFrameNo = decodedFrameNo = 0;
    }
};
extern std::mutex client_buf_mutex;
};  // namespace android

#endif  // HW_EMULATOR_CAMERA_VIRTUALD_CAMERA_FACTORY_H_K
