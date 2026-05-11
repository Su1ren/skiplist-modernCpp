#include "../skiplist.h"
#include "test_utils.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <cstdio>

using namespace skiplist;
// 声明使用的 SkipList 类型，方便后续测试代码编写
using IntStringSkipList = SkipList<int, std::string>;

// 将测试函数封装在匿名命名空间中，避免与其他测试文件中的函数冲突
namespace {

struct StreamBufGuard {
    /**
     * @brief stream buffer guard
     * @details 在构造函数中将目标流的缓冲区替换为提供的缓冲区，在析构函数中恢复原始缓冲区。
     * 这个类用于在测试中捕获 std::cout 的输出，确保测试过程中对 std::cout 的修改不会影响其他测试。
     */
    std::ostream& stream;
    std::streambuf* original;
    // 构造函数：接受一个输出流和一个新的缓冲区，将流的缓冲区替换为新的缓冲区，并保存原始缓冲区以便后续恢复。
    StreamBufGuard(std::ostream& target, std::streambuf* replacement)
        : stream(target), original(target.rdbuf(replacement)) {}

    ~StreamBufGuard() {
        stream.rdbuf(original);
    }
};

SkipListOptions make_options_with_store_file(int max_level, const std::string &store_file) {
    /**
     * @brief 创建 SkipListOptions 对象的辅助函数，设置指定的 max_level 和 store_file 路径。
     * @details 创建一个 SkipListOptions 对象，设置 max_level 和 store_file 成员，并返回该对象。这个函数用于在测试中创建具有特定配置的 SkipList 实例。
     * @return SkipListOptions 
     */
    SkipListOptions options(max_level);
    options.store_file = store_file;
    options.wal_path = store_file + ".wal";
    return options;
}

void remove_file_if_exists(const std::string& path) {
    std::remove(path.c_str());
}

void reset_file(const std::string& path) {
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw TestFailureException("failed to reset file: " + path);
    }
}

std::vector<std::string> read_dump_lines(const std::string& path = kDumpFilePath) {
    std::ifstream dump_file(path);
    if (!dump_file.is_open()) {
        throw TestFailureException("failed to open dump file for reading: " + path);
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(dump_file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::vector<int> read_dump_keys(const std::string& path = kDumpFilePath) {
    const std::vector<std::string> lines = read_dump_lines(path);
    std::vector<int> keys;

    for (const std::string& line : lines) {
        const std::size_t delimiter_pos = line.find(':');
        if (delimiter_pos == std::string::npos) {
            throw TestFailureException("dump file line missing ':' delimiter");
        }
        keys.push_back(std::stoi(line.substr(0, delimiter_pos)));
    }

    return keys;
}

int count_levels_from_display(const IntStringSkipList& skip_list) {
    std::ostringstream output;
    skip_list.display_list(output);

    std::istringstream input(output.str());
    std::string line;
    int level_count = 0;
    while (std::getline(input, line)) {
        if (line.rfind("Level ", 0) == 0) {
            ++level_count;
        }
    }
    return level_count;
}

void test_empty_search_and_delete_smoke() {
    reset_dump_file();
    IntStringSkipList skip_list(6);

    EXPECT_FALSE(skip_list.search_element(42));
    skip_list.delete_element(42);
    EXPECT_EQ(skip_list.size(), 0);
}

void test_insert_and_size() {
    reset_dump_file();
    IntStringSkipList skip_list(6);

    EXPECT_EQ(skip_list.insert_element(3, "c"), 0);
    EXPECT_EQ(skip_list.insert_element(1, "a"), 0);
    EXPECT_EQ(skip_list.insert_element(2, "b"), 0);

    EXPECT_TRUE(skip_list.search_element(1));
    EXPECT_TRUE(skip_list.search_element(2));
    EXPECT_TRUE(skip_list.search_element(3));
    EXPECT_EQ(skip_list.size(), 3);
}

void test_duplicate_insert_keeps_old_semantics() {
    reset_dump_file();
    IntStringSkipList skip_list(6);

    EXPECT_EQ(skip_list.insert_element(7, "first"), 0);
    EXPECT_EQ(skip_list.insert_element(7, "second"), 1);
    EXPECT_TRUE(skip_list.search_element(7));
    EXPECT_EQ(skip_list.size(), 1);
}

void test_delete_existing_and_missing() {
    reset_dump_file();
    IntStringSkipList skip_list(6);

    EXPECT_EQ(skip_list.insert_element(1, "a"), 0);
    EXPECT_EQ(skip_list.insert_element(2, "b"), 0);
    EXPECT_EQ(skip_list.insert_element(3, "c"), 0);

    skip_list.delete_element(2);
    EXPECT_TRUE(skip_list.search_element(1));
    EXPECT_FALSE(skip_list.search_element(2));
    EXPECT_TRUE(skip_list.search_element(3));
    EXPECT_EQ(skip_list.size(), 2);

    skip_list.delete_element(99);
    EXPECT_EQ(skip_list.size(), 2);
}

void test_dump_and_load_round_trip() {
    /**
     * @brief 测试跳表的持久化功能，验证 dump_file 和 load_file 的正确性
     * @details 反复创建跳表实例，插入元素，调用 dump_file 将数据写入文件，然后创建新的跳表实例，调用 load_file 从文件加载数据，并验证加载的数据是否正确。
     * @note 这个测试验证了跳表的持久化功能是否正常工作，确保数据能够正确地写入文件并从文件中读取回来。
     */
    reset_dump_file();
    {
        IntStringSkipList skip_list(6);
        EXPECT_EQ(skip_list.insert_element(5, "five"), 0);
        EXPECT_EQ(skip_list.insert_element(8, "eight"), 0);
        EXPECT_EQ(skip_list.insert_element(13, "thirteen"), 0);
        skip_list.dump_file();
    }

    {
        IntStringSkipList loaded_skip_list(6);
        loaded_skip_list.load_file();

        EXPECT_TRUE(loaded_skip_list.search_element(5));
        EXPECT_TRUE(loaded_skip_list.search_element(8));
        EXPECT_TRUE(loaded_skip_list.search_element(13));
        EXPECT_EQ(loaded_skip_list.size(), 3);
    }
}

void test_level0_order_via_dump_file() {
    reset_dump_file();
    IntStringSkipList skip_list(6);

    EXPECT_EQ(skip_list.insert_element(19, "nineteen"), 0);
    EXPECT_EQ(skip_list.insert_element(1, "one"), 0);
    EXPECT_EQ(skip_list.insert_element(7, "seven"), 0);
    EXPECT_EQ(skip_list.insert_element(3, "three"), 0);
    skip_list.dump_file();

    const std::vector<int> keys = read_dump_keys();
    const std::vector<int> expected_keys = {1, 3, 7, 19};

    EXPECT_EQ(keys.size(), expected_keys.size());
    for (std::size_t i = 0; i < expected_keys.size(); ++i) {
        EXPECT_EQ(keys[i], expected_keys[i]);
    }
}

void test_single_element_delete() {
    reset_dump_file();
    IntStringSkipList skip_list(6);

    EXPECT_EQ(skip_list.insert_element(11, "only"), 0);
    EXPECT_EQ(skip_list.size(), 1);

    skip_list.delete_element(11);
    EXPECT_FALSE(skip_list.search_element(11));
    EXPECT_EQ(skip_list.size(), 0);
}

void test_delete_can_reduce_skip_list_levels() {
    reset_dump_file();
    IntStringSkipList skip_list(8);
    std::vector<int> inserted_keys;

    int initial_level_count = 1;
    for (int key = 0; key < 2048 && initial_level_count == 1; ++key) {
        EXPECT_EQ(skip_list.insert_element(key, "v"), 0);
        inserted_keys.push_back(key);
        initial_level_count = count_levels_from_display(skip_list);
    }

    EXPECT_TRUE(initial_level_count > 1);

    bool saw_level_reduction = false;
    int previous_level_count = initial_level_count;
    for (auto it = inserted_keys.rbegin(); it != inserted_keys.rend(); ++it) {
        skip_list.delete_element(*it);
        const int current_level_count = count_levels_from_display(skip_list);
        if (current_level_count < previous_level_count) {
            saw_level_reduction = true;
        }
        previous_level_count = current_level_count;
    }

    EXPECT_TRUE(saw_level_reduction);
    EXPECT_EQ(skip_list.size(), 0);
    EXPECT_EQ(previous_level_count, 1);
}

void test_load_file_on_empty_dump_file() {
    reset_dump_file();
    IntStringSkipList skip_list(6);

    skip_list.load_file();
    EXPECT_EQ(skip_list.size(), 0);
    EXPECT_FALSE(skip_list.search_element(1));
}

void test_custom_snapshot_path_round_trip() {
    const std::string custom_path = "/tmp/skiplist_cpp_custom_round_trip.dump";
    remove_file_if_exists(custom_path);
    remove_file_if_exists(custom_path + ".wal");

    {
        IntStringSkipList skip_list(make_options_with_store_file(6, custom_path));
        EXPECT_EQ(skip_list.insert_element(21, "twenty-one"), 0);
        EXPECT_EQ(skip_list.insert_element(34, "thirty-four"), 0);
        skip_list.dump_file();
    }

    const std::vector<int> keys = read_dump_keys(custom_path);
    const std::vector<int> expected_keys = {21, 34};
    EXPECT_EQ(keys.size(), expected_keys.size());
    for (std::size_t i = 0; i < expected_keys.size(); ++i) {
        EXPECT_EQ(keys[i], expected_keys[i]);
    }

    {
        IntStringSkipList loaded_skip_list(make_options_with_store_file(6, custom_path));
        loaded_skip_list.load_file();
        EXPECT_TRUE(loaded_skip_list.search_element(21));
        EXPECT_TRUE(loaded_skip_list.search_element(34));
        EXPECT_EQ(loaded_skip_list.size(), 2);
    }

    remove_file_if_exists(custom_path);
    remove_file_if_exists(custom_path + ".wal");
}

void test_two_instances_with_different_snapshot_paths_do_not_interfere() {
    const std::string path_a = "/tmp/skiplist_cpp_path_a.dump";
    const std::string path_b = "/tmp/skiplist_cpp_path_b.dump";
    remove_file_if_exists(path_a);
    remove_file_if_exists(path_b);
    remove_file_if_exists(path_a + ".wal");
    remove_file_if_exists(path_b + ".wal");

    {
        IntStringSkipList list_a(make_options_with_store_file(6, path_a));
        IntStringSkipList list_b(make_options_with_store_file(6, path_b));

        EXPECT_EQ(list_a.insert_element(1, "one"), 0);
        EXPECT_EQ(list_a.insert_element(2, "two"), 0);
        EXPECT_EQ(list_b.insert_element(7, "seven"), 0);
        EXPECT_EQ(list_b.insert_element(8, "eight"), 0);

        list_a.dump_file();
        list_b.dump_file();
    }

    {
        IntStringSkipList loaded_a(make_options_with_store_file(6, path_a));
        IntStringSkipList loaded_b(make_options_with_store_file(6, path_b));
        loaded_a.load_file();
        loaded_b.load_file();

        EXPECT_TRUE(loaded_a.search_element(1));
        EXPECT_TRUE(loaded_a.search_element(2));
        EXPECT_FALSE(loaded_a.search_element(7));
        EXPECT_FALSE(loaded_a.search_element(8));
        EXPECT_EQ(loaded_a.size(), 2);

        EXPECT_TRUE(loaded_b.search_element(7));
        EXPECT_TRUE(loaded_b.search_element(8));
        EXPECT_FALSE(loaded_b.search_element(1));
        EXPECT_FALSE(loaded_b.search_element(2));
        EXPECT_EQ(loaded_b.size(), 2);
    }

    remove_file_if_exists(path_a);
    remove_file_if_exists(path_b);
    remove_file_if_exists(path_a + ".wal");
    remove_file_if_exists(path_b + ".wal");
}

void test_display_list_writes_to_provided_stream_only() {
    reset_dump_file();
    IntStringSkipList skip_list(6);
    EXPECT_EQ(skip_list.insert_element(1, "one"), 0);
    EXPECT_EQ(skip_list.insert_element(2, "two"), 0);

    std::ostringstream custom_stream;
    std::ostringstream captured_stdout;
    {
        StreamBufGuard cout_guard(std::cout, captured_stdout.rdbuf());
        skip_list.display_list(custom_stream);
    }

    EXPECT_TRUE(captured_stdout.str().empty());
    EXPECT_FALSE(custom_stream.str().empty());
    EXPECT_TRUE(custom_stream.str().find("Level 0:") != std::string::npos);
    EXPECT_TRUE(custom_stream.str().find("1:one;") != std::string::npos);
    EXPECT_TRUE(custom_stream.str().find("2:two;") != std::string::npos);
}

void test_iterative_destructor_stress() {
    reset_dump_file();
    {
        IntStringSkipList skip_list(8);
        constexpr int kElementCount = 50000;
        for (int i = 0; i < kElementCount; ++i) {
            EXPECT_EQ(skip_list.insert_element(i, "v"), 0);
        }
        EXPECT_EQ(skip_list.size(), kElementCount);
    }

    EXPECT_TRUE(true);
}

}  // namespace

int main() {
    constexpr std::array<TestCase, 13> test_cases = {{
        {"test_empty_search_and_delete_smoke", test_empty_search_and_delete_smoke},
        {"test_insert_and_size", test_insert_and_size},
        {"test_duplicate_insert_keeps_old_semantics", test_duplicate_insert_keeps_old_semantics},
        {"test_delete_existing_and_missing", test_delete_existing_and_missing},
        {"test_dump_and_load_round_trip", test_dump_and_load_round_trip},
        {"test_level0_order_via_dump_file", test_level0_order_via_dump_file},
        {"test_single_element_delete", test_single_element_delete},
        {"test_delete_can_reduce_skip_list_levels", test_delete_can_reduce_skip_list_levels},
        {"test_load_file_on_empty_dump_file", test_load_file_on_empty_dump_file},
        {"test_custom_snapshot_path_round_trip", test_custom_snapshot_path_round_trip},
        {"test_two_instances_with_different_snapshot_paths_do_not_interfere",
         test_two_instances_with_different_snapshot_paths_do_not_interfere},
        {"test_display_list_writes_to_provided_stream_only", test_display_list_writes_to_provided_stream_only},
        {"test_iterative_destructor_stress", test_iterative_destructor_stress},
    }};

    int passed = 0;

    for (const TestCase& test_case : test_cases) {
        try {
            test_case.test_func();
            ++passed;
            std::cout << "[PASS] " << test_case.name << '\n';
        } catch (const TestFailureException& ex) {
            std::cout << "[FAIL] " << test_case.name << ": " << ex.what() << '\n';
        } catch (const std::exception& ex) {
            std::cout << "[FAIL] " << test_case.name << ": unexpected exception: " << ex.what() << '\n';
        } catch (...) {
            std::cout << "[FAIL] " << test_case.name << ": unknown exception\n";
        }
    }

    std::cout << "\nSummary: " << passed << "/" << test_cases.size() << " passed\n";
    return passed == static_cast<int>(test_cases.size()) ? 0 : 1;
}
