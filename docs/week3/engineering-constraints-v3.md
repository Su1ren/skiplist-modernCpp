# Week 3 工程约束 V3

## 1. 目的

这份约束用于冻结 `Week 3` 完成后的工程边界，避免在进入 `Week 4` 前重新引入已经收敛好的并发模型、API 语义和 benchmark 入口问题。

它与前两版约束的关系是：

1. `V1` 约束最小测试骨架和基线回归。
2. `V2` 约束 P0 工程化改造后的边界。
3. `V3` 约束 P1 并发、API、scan 和 benchmark 已落地后的代码基线。

## 2. 当前阶段的稳定前提

进入 `Week 4` 前，默认接受下面这些已经落地的前提：

1. 并发模型以 `std::shared_mutex + shared_lock/unique_lock` 为核心。
2. `put/get/contains/erase/scan` 是新的主接口。
3. `insert_element/search_element/delete_element` 只作为兼容层保留。
4. benchmark 已切换到参数化 `std::thread` harness。
5. 并发 smoke test 已纳入默认测试集。

## 3. Week 3 之后新增约束

### 3.1 API 语义约束

1. `put` 的默认语义固定为“重复 key 更新旧值”。
2. `insert_element` 的兼容语义固定为“重复 key 返回失败，不更新旧值”。
3. 如果后续调整任何写路径语义，必须同时更新：
   - 新主 API 测试
   - 旧兼容接口测试
4. `scan(begin, end)` 继续保持半开区间 `[begin, end)`。

### 3.2 并发模型约束

1. 当前并发模型定义为“全表级读写锁”，后续讨论必须以此为前提，不要把当前实现表述成 lock-free 或细粒度节点锁模型。
2. 读路径新增接口时，默认先考虑 `shared_lock`。
3. 写路径新增接口时，默认先考虑 `unique_lock`。
4. 如果后续要改变锁粒度，必须同步补并发测试，而不是只改实现。

### 3.3 benchmark 入口约束

1. `stress-test/stress_test.cpp` 已经是参数化 harness，不再回退到写死线程数和 workload 的脚本式压测。
2. benchmark 参数入口保持以下集合：
   - `--threads`
   - `--ops`
   - `--key-space`
   - `--workload`
   - `--seed`
   - `--scan-width`
   - `--sync-wal`
3. `make stress` 默认应编译并执行 benchmark。
4. 需要自定义 benchmark 参数时，统一通过 `STRESS_ARGS` 或脚本透传，不再新增平行入口。

### 3.4 测试体系约束

1. `tests/test_main.cpp` 仍是当前默认单二进制回归入口。
2. 当前与 `Week 3` 直接相关的主接口测试和并发 smoke test 不得回删。
3. 如果改动 `put/get/erase/scan`，至少要跑：
   - 新主 API 语义测试
   - 兼容层语义测试
   - 并发 smoke test

### 3.5 代码层线程 API 约束

1. 仓库代码层面不再直接引入 `pthread` API。
2. `-pthread` 仍保留为编译/链接选项，但只作为工具链层依赖，不再作为代码层线程抽象。
3. 如果出现新的线程入口需求，统一使用 `std::thread` 风格实现。

## 4. 当前阶段明确不做的事情

在进入 `Week 4` 之前，下面这些事情仍不属于 `Week 3` 完成态：

1. 不引入 WAL 真正写路径
2. 不引入 snapshot + WAL 的恢复顺序
3. 不引入校验和与版本号持久化协议
4. 不引入更复杂的 benchmark 结果统计，例如 P50/P99

## 5. Week 3 完成后的最小回归命令

进入 `Week 4` 前，建议至少执行：

```bash
make test
make
make stress STRESS_ARGS="--threads=2 --ops=1000 --key-space=200 --workload=mixed --seed=7 --scan-width=20"
```

如果这里任意一步失败，就不应继续推进 `Week 4` 的 WAL 和恢复链路改造。

## 6. 一句话结论

`Week 3` 之后的代码基线，已经不是“能跑的 KV Demo”，而是“具备明确并发模型、双轨 API 边界、范围扫描能力和参数化 benchmark harness 的 P1 版本”。`Week 4` 的 WAL 与恢复改造应当建立在这个基线之上。
