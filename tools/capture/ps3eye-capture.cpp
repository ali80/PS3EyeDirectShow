#define NOMINMAX
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <thread>
#include <vector>
#include "ps3eye.h"
#include "libusb.h"

// USB topology is stable while cameras remain in the same ports. Never fall
// back to a different camera when the requested port is absent.
static std::string portID(libusb_device* device) {
    uint8_t ports[8] = {};
    int count = libusb_get_port_numbers(device, ports, sizeof(ports));
    if (count <= 0) return {};
    std::string id = "b" + std::to_string(libusb_get_bus_number(device));
    for (int i = 0; i < count; ++i) id += (i == 0 ? "_p" : ".") + std::to_string(ports[i]);
    return id;
}
static uint8_t clampByte(int n) { return static_cast<uint8_t>(std::max(0, std::min(255, n))); }
static std::atomic<bool> stopping{false};
static void toYUV(const std::vector<uint8_t>& rgb, std::vector<uint8_t>& yuv) {
    constexpr int width = 640, height = 480, pixels = width * height;
    for (int y = 0; y < height; y += 2) for (int x = 0; x < width; x += 2) {
        int red = 0, green = 0, blue = 0;
        for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) {
            int p = (y + dy) * width + x + dx;
            int b = rgb[p * 3], g = rgb[p * 3 + 1], r = rgb[p * 3 + 2];
            yuv[p] = clampByte(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
            red += r; green += g; blue += b;
        }
        red /= 4; green /= 4; blue /= 4;
        int chroma = (y / 2) * (width / 2) + x / 2;
        yuv[pixels + chroma] = clampByte(((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128);
        yuv[pixels + pixels / 4 + chroma] = clampByte(((112 * red - 94 * green - 18 * blue + 128) >> 8) + 128);
    }
}
static int selfTest() {
    std::vector<uint8_t> bgr(640 * 480 * 3), yuv(640 * 480 * 3 / 2);
    for (const int value : {0, 255}) {
        std::fill(bgr.begin(), bgr.end(), static_cast<uint8_t>(value));
        toYUV(bgr, yuv);
        if (!std::all_of(yuv.begin(), yuv.begin() + 640 * 480, [value](uint8_t n) { return n == (value ? 235 : 16); })) return 1;
        if (!std::all_of(yuv.begin() + 640 * 480, yuv.end(), [](uint8_t n) { return n == 128; })) return 1;
    }
    std::puts("YUV conversion checks passed");
    return 0;
}
int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--self-test") == 0) return selfTest();
    const bool listing = argc == 2 && std::strcmp(argv[1], "--list") == 0;
    const bool capture = argc == 3 && std::strcmp(argv[1], "--device") == 0;
    if (!listing && !capture) { std::fputs("Use --list or --device <USB-port-id>\n", stderr); return 2; }
    auto devices = ps3eye::PS3EYECam::getDevices();
    if (listing) {
        std::puts("[");
        bool first = true;
        for (const auto& device : devices) {
            const auto id = portID(device->getDevice());
            if (id.empty()) continue;
            std::printf("%s{\"id\":\"%s\",\"name\":\"PS3 Eye\"}", first ? "" : ",\n", id.c_str());
            first = false;
        }
        std::puts("\n]");
        return 0;
    }
    ps3eye::PS3EYECam::PS3EYERef selected;
    for (const auto& device : devices) if (portID(device->getDevice()) == argv[2]) selected = device;
    if (!selected) { std::fputs("Requested camera port is absent\n", stderr); return 3; }
    if (!selected->init(640, 480, 15, ps3eye::PS3EYECam::EOutputFormat::BGR)) {
        std::fputs("Camera initialization failed; check WinUSB binding and other camera apps\n", stderr); return 4;
    }
    if (selected->getWidth() != 640 || selected->getHeight() != 480 || selected->getFrameRate() != 15) return 5;
    selected->setAutogain(true);
    selected->setAutoWhiteBalance(true);
    if (_setmode(_fileno(stdout), _O_BINARY) < 0) return 6;
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::vector<uint8_t> bgr(640 * 480 * 3), yuv(640 * 480 * 3 / 2);
    selected->start();
    std::thread([] {
        std::getchar(); // A stop byte or parent pipe closure both end capture.
        stopping.store(true);
        std::this_thread::sleep_for(std::chrono::seconds(3));
        // A disconnected camera can block the native getFrame call. Do not
        // leave an orphan helper behind if the parent has exited or crashed.
        std::_Exit(7);
    }).detach();
    // Parent owns this process and cancels it when its bounded capture lease ends.
    // No files, network access, or audio playback are used by this helper.
    while (!stopping.load()) {
        selected->getFrame(bgr.data());
        if (stopping.load()) break;
        toYUV(bgr, yuv);
        if (std::fwrite(yuv.data(), 1, yuv.size(), stdout) != yuv.size()) break;
    }
    selected->stop();
    return 0;
}
