#pragma once
#include <atomic>
#include <iostream>

namespace eng {

class ResourceTracker {
public:
    static void incBuffer() { aliveBuffers++; }
    static void decBuffer() { aliveBuffers--; }
    static void incImage()  { aliveImages++; }
    static void decImage()  { aliveImages--; }

    static void validateAtShutdown() {
        if (aliveBuffers != 0 || aliveImages != 0) {
            std::cerr << "!!! RESOURCE LEAK DETECTED !!!" << std::endl;
            std::cerr << "Buffers: " << aliveBuffers << std::endl;
            std::cerr << "Images:  " << aliveImages << std::endl;
        } else {
            std::cout << "All Vulkan resources properly destroyed." << std::endl;
        }
    }

private:
    static inline std::atomic<int> aliveBuffers{0};
    static inline std::atomic<int> aliveImages{0};
};

} // namespace eng
