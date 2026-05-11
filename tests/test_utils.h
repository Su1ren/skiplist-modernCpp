#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

inline constexpr const char* kDumpFilePath = "store/dumpFile";

class TestFailureException : public std::runtime_error {
public:
    explicit TestFailureException(const std::string& message)
        : std::runtime_error(message) {}
};

#define EXPECT_TRUE(expr)                                                       \
    do {                                                                        \
        if (!(expr)) {                                                          \
            std::ostringstream oss;                                             \
            oss << "EXPECT_TRUE failed: " << #expr << " at " << __FILE__ << ":" \
                << __LINE__;                                                    \
            throw TestFailureException(oss.str());                              \
        }                                                                       \
    } while (0)

#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))

#define EXPECT_EQ(lhs, rhs)                                                      \
    do {                                                                         \
        const auto& _lhs = (lhs);                                                \
        const auto& _rhs = (rhs);                                                \
        if (!(_lhs == _rhs)) {                                                   \
            std::ostringstream oss;                                              \
            oss << "EXPECT_EQ failed: " << #lhs << " != " << #rhs << " ("        \
                << _lhs << " vs " << _rhs << ") at " << __FILE__ << ":"          \
                << __LINE__;                                                     \
            throw TestFailureException(oss.str());                               \
        }                                                                        \
    } while (0)

inline void reset_dump_file() {
    std::ofstream dump_file(kDumpFilePath, std::ios::trunc);
    if (!dump_file.is_open()) {
        throw TestFailureException("failed to reset dump file");
    }
}

struct TestCase {
    const char* name;
    void (*test_func)();
};

#endif  // TEST_UTILS_H
