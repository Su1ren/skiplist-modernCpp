/* ************************************************************************
> File Name:     stress_test.cpp
> Author:        程序员Carl
> 微信公众号:    代码随想录
> Created Time:  Sun 16 Dec 2018 11:56:04 AM CST
> Description:   Parameterized benchmark harness for SkipList workloads.
 ************************************************************************/

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "../skiplist.h"

using namespace skiplist;

namespace {

// 定义工作负载类型枚举，表示不同的测试场景：纯写入、纯读取、混合读写和范围扫描。
enum class Workload : uint8_t {
    put, // 纯写入工作负载，所有操作都是插入或更新元素。
    get, // 纯读取工作负载，所有操作都是查询元素。
    mixed, // 兼有读写的混合工作负载，操作类型交替进行。
    scan, // 范围扫描工作负载，所有操作都是在跳表中执行范围查询。
};

// 定义跳表基准测试选项结构体，包含线程数、操作数、键空间大小、工作负载类型、随机种子、扫描宽度和是否同步 WAL。
struct BenchmarkOptions {
    int threads = 4; // 线程数，默认值为 4，表示基准测试将使用 4 个并发线程执行操作。
    std::size_t ops = 100000; // 操作数，默认值为 100000，表示每轮测试中将执行 100000 个操作。
    int key_space = 100000; // 键空间大小，默认值为 100000，表示键的取值范围。
    Workload workload = Workload::put; // 工作负载类型，默认值为 put。
    std::uint32_t seed = 12345; // 随机种子，默认值为 12345。
    int scan_width = 100; // 扫描宽度，默认值为 100。
    bool sync_wal = false; // 是否同步 WAL，默认值为 false。
};

// 定义每轮测试结果结构体，包含耗时、吞吐量和观察到的操作数量。
struct RoundResult {
    double elapsed_ms = 0.0;
    double throughput_ops_per_s = 0.0;
    std::uint64_t observed = 0;
};

[[noreturn]] void print_usage_and_exit(const char *program, const std::string &error) {
    /**
     * @brief 打印使用说明并退出程序
     * @param program：程序名称，通常是 argv[0]。
     * @param error：错误信息，如果不为空，则在使用说明前打印错误信息。
     * @details 该函数用于在用户提供无效参数时向用户显示正确的使用方法，并退出程序。它首先检查是否有错误信息需要打印，然后输出程序的使用说明，包括可用的命令行选项和参数。最后调用 std::exit(2) 以非零状态退出程序，表示发生了错误。
     */
    if (!error.empty()) {
        std::cerr << "Error: " << error << "\n\n";
    }

    std::cerr
        << "Usage: " << program << " [options]\n"
        << "  --threads N\n"
        << "  --ops N\n"
        << "  --key-space N\n"
        << "  --workload=put|get|mixed|scan\n"
        << "  --seed N\n"
        << "  --scan-width N\n"
        << "  --sync-wal\n";
    std::exit(2);
}

Workload parse_workload(const std::string& value) {
    if (value == "put") {
        return Workload::put;
    }
    if (value == "get") {
        return Workload::get;
    }
    if (value == "mixed") {
        return Workload::mixed;
    }
    if (value == "scan") {
        return Workload::scan;
    }
    throw std::invalid_argument("unknown workload: " + value);
}

const char* to_string(Workload workload) {
    switch (workload) {
        case Workload::put:
            return "put";
        case Workload::get:
            return "get";
        case Workload::mixed:
            return "mixed";
        case Workload::scan:
            return "scan";
    }
    return "unknown";
}

std::size_t parse_size_option(const std::string& flag, const std::string& value) {
    try {
        const std::size_t parsed = std::stoull(value);
        if (parsed == 0) {
            throw std::invalid_argument(flag + " must be greater than 0");
        }
        return parsed;
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid value for " + flag + ": " + value);
    }
}

int parse_int_option(const std::string& flag, const std::string& value) {
    try {
        const int parsed = std::stoi(value);
        if (parsed <= 0) {
            throw std::invalid_argument(flag + " must be greater than 0");
        }
        return parsed;
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid value for " + flag + ": " + value);
    }
}

std::uint32_t parse_seed_option(const std::string& value) {
    try {
        return static_cast<std::uint32_t>(std::stoul(value));
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid value for --seed: " + value);
    }
}

BenchmarkOptions parse_args(int argc, char** argv) {
    BenchmarkOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        auto read_next_value = [&](const std::string& flag) -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument("missing value for " + flag);
            }
            return argv[++i];
        };

        if (arg == "--threads") {
            options.threads = parse_int_option(arg, read_next_value(arg));
        } else if (arg.rfind("--threads=", 0) == 0) {
            options.threads = parse_int_option("--threads", arg.substr(10));
        } else if (arg == "--ops") {
            options.ops = parse_size_option(arg, read_next_value(arg));
        } else if (arg.rfind("--ops=", 0) == 0) {
            options.ops = parse_size_option("--ops", arg.substr(6));
        } else if (arg == "--key-space") {
            options.key_space = parse_int_option(arg, read_next_value(arg));
        } else if (arg.rfind("--key-space=", 0) == 0) {
            options.key_space = parse_int_option("--key-space", arg.substr(12));
        } else if (arg == "--workload") {
            options.workload = parse_workload(read_next_value(arg));
        } else if (arg.rfind("--workload=", 0) == 0) {
            options.workload = parse_workload(arg.substr(11));
        } else if (arg == "--seed") {
            options.seed = parse_seed_option(read_next_value(arg));
        } else if (arg.rfind("--seed=", 0) == 0) {
            options.seed = parse_seed_option(arg.substr(7));
        } else if (arg == "--scan-width") {
            options.scan_width = parse_int_option(arg, read_next_value(arg));
        } else if (arg.rfind("--scan-width=", 0) == 0) {
            options.scan_width = parse_int_option("--scan-width", arg.substr(13));
        } else if (arg == "--sync-wal") {
            options.sync_wal = true;
        } else if (arg == "--sync-wal=true") {
            options.sync_wal = true;
        } else if (arg == "--sync-wal=false") {
            options.sync_wal = false;
        } else {
            throw std::invalid_argument("unknown argument: " + arg);
        }
    }

    return options;
}

std::vector<std::size_t> split_operations(std::size_t ops, int threads) {
    std::vector<std::size_t> counts(static_cast<std::size_t>(threads), ops / threads);
    const std::size_t remainder = ops % static_cast<std::size_t>(threads);
    for (std::size_t i = 0; i < remainder; ++i) {
        ++counts[i];
    }
    return counts;
}

std::string make_value(int key, std::size_t sequence) {
    return "v" + std::to_string(key) + "_" + std::to_string(sequence);
}

void prefill_for_read_heavy_workloads(SkipList<int, std::string>& skip_list, int key_space) {
    for (int key = 0; key < key_space; ++key) {
        skip_list.put(key, "seed");
    }
}

RoundResult run_round(const BenchmarkOptions& benchmark_options, std::uint32_t round_seed) {
    SkipListOptions list_options(18);
    list_options.enable_wal = benchmark_options.sync_wal;
    list_options.sync_wal = benchmark_options.sync_wal;
    list_options.enable_debug_output = false;

    SkipList<int, std::string> skip_list(list_options);
    if (benchmark_options.workload == Workload::get ||
        benchmark_options.workload == Workload::mixed ||
        benchmark_options.workload == Workload::scan) {
        prefill_for_read_heavy_workloads(skip_list, benchmark_options.key_space);
    }

    const std::vector<std::size_t> ops_per_thread =
        split_operations(benchmark_options.ops, benchmark_options.threads);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(benchmark_options.threads));
    std::vector<std::uint64_t> observed(static_cast<std::size_t>(benchmark_options.threads), 0);

    const auto start = std::chrono::high_resolution_clock::now();
    for (int thread_index = 0; thread_index < benchmark_options.threads; ++thread_index) {
        threads.emplace_back([&, thread_index]() {
            std::mt19937 rng(round_seed + static_cast<std::uint32_t>(thread_index * 9973));
            std::uniform_int_distribution<int> key_dist(0, benchmark_options.key_space - 1);
            std::uint64_t local_observed = 0;
            const std::size_t local_ops = ops_per_thread[static_cast<std::size_t>(thread_index)];

            for (std::size_t op_index = 0; op_index < local_ops; ++op_index) {
                switch (benchmark_options.workload) {
                    case Workload::put: {
                        const int key = key_dist(rng);
                        const WriteResult result = skip_list.put(key, make_value(key, op_index));
                        if (result == WriteResult::inserted) {
                            ++local_observed;
                        }
                        break;
                    }
                    case Workload::get: {
                        const int key = key_dist(rng);
                        if (skip_list.get(key).has_value()) {
                            ++local_observed;
                        }
                        break;
                    }
                    case Workload::mixed: {
                        const int key = key_dist(rng);
                        if ((op_index % 2U) == 0U) {
                            const WriteResult result = skip_list.put(key, make_value(key, op_index));
                            if (result == WriteResult::inserted) {
                                ++local_observed;
                            }
                        } else if (skip_list.get(key).has_value()) {
                            ++local_observed;
                        }
                        break;
                    }
                    case Workload::scan: {
                        const int begin = key_dist(rng);
                        const int end = std::min(begin + benchmark_options.scan_width,
                                                 benchmark_options.key_space);
                        local_observed +=
                            static_cast<std::uint64_t>(skip_list.scan(begin, end).size());
                        break;
                    }
                }
            }

            observed[static_cast<std::size_t>(thread_index)] = local_observed;
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }
    const auto finish = std::chrono::high_resolution_clock::now();

    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(finish - start).count();
    const double throughput =
        (static_cast<double>(benchmark_options.ops) * 1000.0) / elapsed_ms;

    RoundResult result;
    result.elapsed_ms = elapsed_ms;
    result.throughput_ops_per_s = throughput;
    for (std::uint64_t value : observed) {
        result.observed += value;
    }
    return result;
}

double median_throughput(const std::vector<RoundResult> &round_results) {
    /**
     * @brief 计算多轮测试结果的中位数吞吐量
     * @param round_results 多轮测试结果的向量
     * @return 中位数吞吐量，单位为操作数每秒
     * @details 该函数首先从每轮测试结果中提取吞吐量值，并将它们存储在一个新的向量中。然后对这个向量进行排序，并返回中间位置的值作为中位数吞吐量。中位数吞吐量可以更好地反映测试结果的典型性能，避免极端值对结果的影响。
     */
    std::vector<double> values;
    values.reserve(round_results.size());
    for (const RoundResult& result : round_results) {
        values.push_back(result.throughput_ops_per_s);
    }
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

void print_config(const BenchmarkOptions& options) {
    std::cout << "Benchmark config\n";
    std::cout << "  workload: " << to_string(options.workload) << "\n";
    std::cout << "  threads: " << options.threads << "\n";
    std::cout << "  ops: " << options.ops << "\n";
    std::cout << "  key-space: " << options.key_space << "\n";
    std::cout << "  seed: " << options.seed << "\n";
    std::cout << "  scan-width: " << options.scan_width << "\n";
    std::cout << "  sync-wal: " << (options.sync_wal ? "true" : "false") << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    BenchmarkOptions options;
    try {
        options = parse_args(argc, argv);
    } catch (const std::exception& ex) {
        print_usage_and_exit(argv[0], ex.what());
    }
    // 打印基准测试配置，包括工作负载类型、线程数、操作数、键空间大小、随机种子、扫描宽度和是否同步 WAL。
    print_config(options);

    std::vector<RoundResult> round_results;
    round_results.reserve(3);
    for (int round = 0; round < 3; ++round) {
        const std::uint32_t round_seed = options.seed + static_cast<std::uint32_t>(round * 101);
        const RoundResult result = run_round(options, round_seed);
        round_results.push_back(result);
        std::cout << "Round " << (round + 1)
                  << ": elapsed_ms=" << result.elapsed_ms
                  << ", throughput_ops_per_s=" << result.throughput_ops_per_s
                  << ", observed=" << result.observed << "\n";
    }

    std::cout << "Median throughput_ops_per_s: "
              << median_throughput(round_results) << "\n";
    return 0;
}
