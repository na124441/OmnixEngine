#include "Runtime/ErrorTests.h"
#include "Core/Error/Result.h"
#include "Core/Logger.h"
#include <string>
#include <cassert>

namespace eng::test {

    using namespace eng::core;

    // Simple test structs
    struct Person {
        std::string name;
        int age;
    };

    // Helper functions for monadic testing
    static Expected<int, std::string> StringToLength(const std::string& s) {
        if (s.empty()) {
            return std::string("Empty string error");
        }
        return static_cast<int>(s.length());
    }

    static Expected<double, std::string> SqrtOfInt(int val) {
        if (val < 0) {
            return std::string("Negative value error");
        }
        // Just return half for simplicity
        return static_cast<double>(val) * 0.5;
    }

    void RunErrorTests() {
        LOG_INFO("=== Running Kernel Error/Expected Tests ===");

        // 1. Success Value Tests
        {
            LOG_INFO("[Test] Starting Success Value Tests...");
            Expected<int, std::string> res(42);
            assert(res.has_value());
            assert(res);
            assert(res.value() == 42);
            assert(*res == 42);
            LOG_INFO("[Test] Success Value Tests Passed.");
        }

        // 2. Error Containment Tests
        {
            LOG_INFO("[Test] Starting Error Containment Tests...");
            Expected<int, std::string> res = Unexpected<std::string>("Something went wrong");
            assert(!res.has_value());
            assert(!res);
            assert(res.error() == "Something went wrong");
            LOG_INFO("[Test] Error Containment Tests Passed.");
        }

        // 3. Pointer Operator Tests
        {
            LOG_INFO("[Test] Starting Pointer Operator Tests...");
            Expected<Person, int> res(Person{ "John Doe", 30 });
            assert(res.has_value());
            assert(res->name == "John Doe");
            assert(res->age == 30);
            LOG_INFO("[Test] Pointer Operator Tests Passed.");
        }

        // 4. Monadic Chaining Tests (and_then, transform)
        {
            LOG_INFO("[Test] Starting Monadic Chaining Tests...");
            
            // Value path: "Hello" -> length 5 -> sqrt/half 2.5
            Expected<std::string, std::string> initial("Hello");
            auto chainRes = initial
                .and_then(StringToLength)
                .and_then(SqrtOfInt);
                
            assert(chainRes.has_value());
            assert(chainRes.value() == 2.5);

            // Error path (empty string)
            Expected<std::string, std::string> emptyInitial("");
            auto chainErr = emptyInitial
                .and_then(StringToLength)
                .and_then(SqrtOfInt);
                
            assert(!chainErr.has_value());
            assert(chainErr.error() == "Empty string error");

            // Transform value
            Expected<int, std::string> vRes(10);
            auto doubleRes = vRes.transform([](int x) { return x * 2; });
            assert(doubleRes.has_value());
            assert(doubleRes.value() == 20);

            LOG_INFO("[Test] Monadic Chaining Tests Passed.");
        }

        // 5. Expected<void, E> Tests
        {
            LOG_INFO("[Test] Starting Void Expected Tests...");
            
            // Void Success
            Expected<void, int> voidSuccess;
            assert(voidSuccess.has_value());
            assert(voidSuccess);
            
            // Void Error
            Expected<void, int> voidError = Unexpected<int>(404);
            assert(!voidError.has_value());
            assert(!voidError);
            assert(voidError.error() == 404);

            LOG_INFO("[Test] Void Expected Tests Passed.");
        }

        LOG_INFO("=== All Error/Expected Tests Passed Successfully ===");
    }

} // namespace eng::test
