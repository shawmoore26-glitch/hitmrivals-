// tests/TestFramework.h
// Deliberately tiny -- registers named test functions and runs them all,
// reporting pass/fail counts and a non-zero exit code on any failure so
// CTest / CI treats a failure as a failure. No external dependency.
#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace dominus::test {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> registry;
    return registry;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        Registry().push_back({name, std::move(fn)});
    }
};

inline int RunAll() {
    int failed = 0;
    for (auto& tc : Registry()) {
        try {
            tc.fn();
            std::cout << "[PASS] " << tc.name << "\n";
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << tc.name << " -- " << e.what() << "\n";
            ++failed;
        } catch (...) {
            std::cout << "[FAIL] " << tc.name << " -- unknown exception\n";
            ++failed;
        }
    }
    std::cout << (Registry().size() - failed) << "/" << Registry().size() << " tests passed\n";
    return failed == 0 ? 0 : 1;
}

}  // namespace dominus::test

#define DOMINUS_TEST(name)                                                     \
    void name();                                                               \
    static ::dominus::test::Registrar registrar_##name(#name, name);           \
    void name()

#define DOMINUS_EXPECT(cond)                                                   \
    if (!(cond)) throw std::runtime_error("DOMINUS_EXPECT failed: " #cond " at " __FILE__)
