# Week 3 改造同步记录

## 1. 目的

这份文档用于把 `docs/skiplist-cpp-4week-implementation-plan.md` 中 `Week 3` 的计划项，与当前仓库里的实际实现同步起来。

重点回答：

1. `Step 1` 到 `Step 6` 当前是否已落地。
2. `Week 3` 的完成标准是否已经满足。
3. 哪些边界已经可以作为 `Week 4` 的稳定基线继续使用。

## 2. 完成标准对照

`Week 3` 的完成标准是：

1. 你可以明确回答“这个项目现在是如何做读写并发控制的”。
2. benchmark 输出不再受每次操作日志影响。
3. `put/get/erase/scan` 构成一套更像 KV 引擎的主接口。
4. 压测与并发 smoke test 已全部切换到 `std::thread`，仓库代码层面不再直接依赖 `pthread` API。

当前状态：

1. `skiplist.h` 中读写路径已经以 `std::shared_mutex + shared_lock/unique_lock` 为主模型。
2. benchmark 入口 [stress_test.cpp](../../stress-test/stress_test.cpp:1) 已重写为参数化 `std::thread` harness。
3. 新主接口 `put/get/contains/erase/scan/checkpoint/recover` 已落地在 [skiplist.h](../../skiplist.h:154) 之后。
4. 并发 smoke test 已补齐到 [tests/test_main.cpp](../../tests/test_main.cpp:365) 的测试集。

结论：

1. 按当前计划定义，`Week 3` 已达到完成状态。

## 3. Step 1 到 Step 6 的实际改造内容

### 3.1 Step 1：升级并发模型

当前落地结果：

1. [SkipList](../../skiplist.h:238) 使用 `mutable std::shared_mutex mtx_` 作为全表级读写锁。
2. 读路径已使用 `std::shared_lock`：
   - [display_list](../../skiplist.h:291)
   - [size() const](../../skiplist.h:544)
   - [get](../../skiplist.h:563)
   - [contains](../../skiplist.h:575)
   - [scan](../../skiplist.h:615)
   - [dump_file](../../skiplist.h:315)
3. 写路径已使用 `std::unique_lock`：
   - [insert_element](../../skiplist.h:280)
   - [erase](../../skiplist.h:607)
   - [put](../../skiplist.h:690)
   - [load_file](../../skiplist.h:331)
   - [checkpoint](../../skiplist.h:642)
   - [recover](../../skiplist.h:664)

当前模型定义：

1. 这不是 lock-free，也不提供无锁迭代器。
2. 当前模型可以明确表述为“全表级读写锁”。

### 3.2 Step 2：重建主 API，旧 API 只做兼容层

当前落地结果：

1. 已新增 [WriteResult](../../skiplist.h:143)，取值为：
   - `inserted`
   - `updated`
2. 已新增主接口声明与实现：
   - [size() const](../../skiplist.h:544)
   - [get](../../skiplist.h:563)
   - [contains](../../skiplist.h:575)
   - [erase](../../skiplist.h:607)
   - [scan](../../skiplist.h:615)
   - [checkpoint](../../skiplist.h:642)
   - [recover](../../skiplist.h:664)
   - [put](../../skiplist.h:690)
3. 旧接口已作为兼容包装：
   - [search_element](../../skiplist.h:392) 调 `contains`
   - [delete_element](../../skiplist.h:380) 调 `erase`
   - [insert_element](../../skiplist.h:280) 保持“仅插入”语义

调用侧迁移结果：

1. demo [main.cpp](../../main.cpp:17) 已改用 `put / contains / erase`。
2. benchmark [stress_test.cpp](../../stress-test/stress_test.cpp:198) 已改用 `put / get / scan`。

### 3.3 Step 3：明确重复 key 策略

当前落地结果：

1. 新主写路径 [put](../../skiplist.h:690) 采用“重复 key 覆盖旧值”的语义。
2. 兼容接口 [insert_element](../../skiplist.h:280) 仍保持“重复 key 返回失败，不更新旧值”的语义。
3. 旧语义已由测试固定：
   - [test_duplicate_insert_keeps_old_semantics](../../tests/test_main.cpp:113)
4. 新语义已由测试固定：
   - [test_put_returns_inserted_then_updated_and_get_returns_latest_value](../../tests/test_main.cpp:365)

当前意义：

1. “新主写路径覆盖旧值”和“旧兼容接口保持旧行为”之间的边界已经清晰。
2. 这也满足了 `Week 3` 里“避免破坏性升级过大”的要求。

### 3.4 Step 4：加入范围扫描能力

当前落地结果：

1. [scan(begin, end)](../../skiplist.h:615) 已按半开区间 `[begin, end)` 实现。
2. 实现逻辑为：
   - 先定位到第一个 `>= begin` 的节点
   - 再沿 level 0 顺序收集
   - 遇到 `key >= end` 终止
3. 扫描结果按 key 升序返回。

测试覆盖：

1. [test_scan_returns_sorted_half_open_range](../../tests/test_main.cpp:392)

### 3.5 Step 5：重写 benchmark，不再沿用“打印每次插入”的压测方式

当前落地结果：

1. [stress_test.cpp](../../stress-test/stress_test.cpp:1) 已不再使用 `pthread` API。
2. benchmark 入口支持参数：
   - `--threads`
   - `--ops`
   - `--key-space`
   - `--workload=put|get|mixed|scan`
   - `--seed`
   - `--scan-width`
   - `--sync-wal`
3. 已支持 4 类 workload：
   - `put`
   - `get`
   - `mixed`
   - `scan`
4. `mixed` 默认是 50% `put` + 50% `get`，实现见 [run_round](../../stress-test/stress_test.cpp:242)。
5. `scan` 默认使用配置里的 `scan_width`，实现见 [run_round](../../stress-test/stress_test.cpp:254)。
6. 每次 benchmark 固定执行 3 轮，并输出：
   - 每轮 `elapsed_ms`
   - 每轮 `throughput_ops_per_s`
   - 中位吞吐 `Median throughput_ops_per_s`
7. [stress_test_start.sh](../../stress_test_start.sh:1) 已支持参数透传。
8. [makefile](../../makefile:14) 的 `make stress` 已改为“编译后执行”，并支持 `STRESS_ARGS` 透传。

一个可复用的运行示例：

```bash
make stress STRESS_ARGS="--threads=2 --ops=1000 --key-space=200 --workload=mixed --seed=7 --scan-width=20"
```

当前意义：

1. benchmark 已经从“写死线程数和操作数的脚本”升级成可配置的 harness。
2. benchmark 输出不再受每次操作日志影响。

### 3.6 Step 6：扩展 P1 测试

当前落地结果：

当前测试入口在 [tests/test_main.cpp](../../tests/test_main.cpp:1)，共 `19` 个 case。

与 `Week 3` 直接相关的新增测试：

1. [test_put_returns_inserted_then_updated_and_get_returns_latest_value](../../tests/test_main.cpp:365)
   - 覆盖 `put + WriteResult + get`
2. [test_contains_erase_and_size_main_api](../../tests/test_main.cpp:376)
   - 覆盖 `contains + erase + size() const`
3. [test_scan_returns_sorted_half_open_range](../../tests/test_main.cpp:392)
   - 覆盖 `scan`
4. [test_concurrent_put_smoke](../../tests/test_main.cpp:408)
   - 覆盖多线程 `put`
5. [test_concurrent_get_smoke](../../tests/test_main.cpp:434)
   - 覆盖多线程 `get`
6. [test_concurrent_mixed_smoke](../../tests/test_main.cpp:468)
   - 覆盖读写混合 smoke test

兼容接口行为仍保持稳定，相关旧接口测试包括：

1. `test_insert_and_size`
2. `test_duplicate_insert_keeps_old_semantics`
3. `test_delete_existing_and_missing`
4. `test_single_element_delete`

验证结果：

1. 当前 `make test` 输出 `19/19 passed`。

## 4. 当前 Week 3 之后的代码状态

从工程形态看，当前仓库已经从“P0 工程化版本”进一步升级到：

1. 有明确的全表级读写锁模型
2. 有新主 API 和兼容层 API 双轨边界
3. 有覆盖重复 key 更新语义的 `WriteResult`
4. 有范围扫描接口
5. 有参数化 benchmark harness
6. 有并发 smoke test

但还没有进入 `Week 4` 的内容：

1. 还没有真正的 WAL 写前日志
2. 还没有 snapshot + WAL 的恢复时序
3. 还没有版本号与 checksum 格式

## 5. 本周验证命令

本轮同步时实际执行并通过的命令：

```bash
make test
make
make stress STRESS_ARGS="--threads=2 --ops=1000 --key-space=200 --workload=mixed --seed=7 --scan-width=20"
```

验证结果：

1. `make test`：`19/19 passed`
2. `make`：通过
3. `make stress ...`：通过，并输出 3 轮统计结果和中位吞吐

## 6. 和下一阶段的衔接

如果继续推进 `Week 4`，建议把当前 `Week 3` 状态作为新的稳定基线：

1. 保留当前 `WriteResult`
2. 保留当前 `put/get/contains/erase/scan` 主接口
3. 在此基础上引入 WAL、checkpoint/recover 真正语义和恢复链路

配套约束见：

1. [Week 3 工程约束 V3](./engineering-constraints-v3.md)
