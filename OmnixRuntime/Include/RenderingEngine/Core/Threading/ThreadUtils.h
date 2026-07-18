#include <cstdint>
#include <thread>
namespace eng {

    inline uint32_t GetHardwareThreadCount() {
        return static_cast<uint32_t>(std::thread::hardware_concurrency());
    }

    // Hint for job system: create N workers = hardware threads - 1 (reserve 1 for main thread)
    inline uint32_t RecommendedWorkerCount() {
        const uint32_t hc = GetHardwareThreadCount();
        return (hc > 1) ? hc - 1 : 1;
    }

    // Simple scoped thread wrapper for quick fire-and-forget tasks (used only in tests)
    class ScopedThread {
    public:
        template <typename Callable>
        explicit ScopedThread(Callable&& fn) : thread_(std::forward<Callable>(fn)) {}
        ~ScopedThread() { if (thread_.joinable()) thread_.join(); }
    private:
        std::thread thread_;
    };

} // namespace eng
