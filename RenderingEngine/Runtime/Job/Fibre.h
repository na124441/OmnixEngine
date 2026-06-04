#include <functional>
class Fiber {
public:
    using EntryFn = std::function<void(void*)>;
    explicit Fiber(EntryFn entry, void* userData);
    ~Fiber();

    void SwitchTo();        // transfer execution to this fiber
    void Yield();           // return to the caller fiber
};
