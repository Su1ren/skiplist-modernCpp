# Skiplist-CPP 四周实施计划（P0-P1 重点版）

## Summary

在 4 周窗口内，目标不是把项目做成完整数据库，而是把它从“跳表 Demo”升级成“可讲工程取舍的轻量级 KV 引擎”。本计划采用小步快跑路线：保留当前 header-only 主体，升级到 C++17，先用轻量测试守住正确性，再集中完成 P0 的工程质量修复和 P1 的并发、API、WAL/恢复、benchmark 改造。P2 只做下一阶段的预览，不在本轮实现。

默认落地决策：
- 升级到 `C++17`
- 保持“小步快跑”，不做大拆分式重构
- P0-P1 先用轻量测试二进制，不先接入 GoogleTest
- 新主语义采用 `put` 覆盖旧值，重复 key 默认更新
- P1 WAL 目标是“基础可恢复版”，不是完整生产级 durability
- 并发代码统一到 C++ 标准库抽象，代码层面使用 `std::mutex` / `std::shared_mutex` / `std::thread`，不再直接使用 `pthread` API

## Week 1：建立基线 + 先把“会讲”变成“可验证”

### 目标
冻结当前行为，补一层最小测试护栏，顺手把面试时必须讲清的设计点整理出来。第 1 周不做大改，只做“为改造做准备”的工作。

### 实现步骤
1. 固化当前代码理解
- 把 `skiplist.h` 的插入、查询、删除、落盘、加载五条路径分别整理成伪代码和时序笔记。
- 重点记录 5 个当前缺陷，后续每周都要回看：
  - 头文件全局 `mtx` / `delimiter`
  - `search_element` 无锁
  - `load_file` 写死 `stoi`
  - 递归析构
  - benchmark 日志污染和单线程默认值

2. 建最小测试骨架
- 新建一个轻量测试二进制，使用 `assert` 或自定义 `EXPECT_TRUE/EXPECT_EQ` 宏，不引入第三方框架。
- 测试入口只做一件事：按 case 顺序运行并打印通过/失败摘要。
- 测试覆盖当前行为，不急着引入新语义。

3. 先写“基线回归测试”
- 插入唯一 key 后可查到，`size` 正确增长。
- 重复 `insert_element` 保持当前语义：不覆盖，元素数不增长。
- 删除存在 key 后不可查；删除不存在 key 不崩溃。
- `dump_file` + `load_file` 能完成基本 round-trip。
- 插入后 level0 结果有序。
- 空表查询、空表删除、单元素表删除能正常返回。

4. 输出第一版工程约束
- 本轮实现中，内存结构仍保持模板化。
- 持久化路径先只支持 `K=int`、`V=std::string`。
- 第 4 周前不做 SSTable / compaction / Bloom Filter。

### 本周完成标准
- 你能不看代码讲清 `update[]`、`_skip_list_level`、`get_random_level`。
- 项目有一套可重复运行的轻量回归测试。
- 已形成后续改造的“红线”：每次重构先跑测试。

## Week 2：P0 工程化改造，把项目先做“正确、干净、像库”

### 目标
本周专注修掉最影响项目成色的问题，不碰复杂存储特性。完成后，这个项目至少像一个可复用组件，而不是教程代码。

### 实现步骤

1. 清理头文件级全局状态
- 在 `skiplist.h` 增加 `#pragma once`。
- 删除头文件里的全局 `mtx` 和 `delimiter`。
- 把同步原语改为 `SkipList` 的成员锁。
- 把分隔符/转义逻辑收回到私有辅助函数，不再暴露为全局可变状态。

2. 引入最小配置对象
- 新增 `SkipListOptions`，至少包含：
  - `int max_level`
  - `std::string snapshot_path`
  - `std::string wal_path`
  - `bool enable_wal`
  - `bool sync_wal`
  - `bool enable_debug_output`
- 保留便捷构造 `SkipList(int max_level)`，内部委托到默认 `SkipListOptions`，避免 demo 立刻全量改动。

3. 规范命名与作用域
- 为实现加命名空间，例如 `skiplist`。
- 保留现有 `Node` / `SkipList` 命名，不做风格大改。
- 宏 `STORE_FILE` 彻底移除，所有文件路径从 options 获取。

4. 去掉库内部日志依赖
- 核心读写路径不再直接 `std::cout`。
- 需要调试输出的功能改成：
  - `display_list(std::ostream&) const`，只负责把内容打印到传入流
  - 或 `to_string_levels() const`，由调用方决定是否打印
- `main.cpp` 负责演示输出，核心库不承担日志职责。

5. 修正析构和内存清理
- 删除递归 `clear(Node*)`。
- 改成只沿 `level 0` 链表迭代释放全部节点，最后释放 header。
- 释放逻辑统一放到析构或私有 `clear_all_nodes()`，避免重复路径。

6. 明确“模板与持久化”的边界
- 内存跳表仍允许模板化。
- `load_file` / `dump_file` / 第 4 周的 WAL，仅在 `K=int, V=std::string` 时启用。
- 通过 `static_assert` 或约束检查显式表达这个限制，不再伪装成“完全泛型持久化”。

7. 扩展 P0 测试
- 两个不同实例使用不同 snapshot 路径，互不干扰。
- 析构压力测试：大量节点插入后析构不发生栈递归问题。
- `display_list` 不依赖标准输出副作用。
- 构造默认路径和自定义路径都可正常工作。

8. 统一构建基线，为 P1 线程模型改造做准备
- 把 `makefile` 和后续测试/benchmark 的编译标准统一到 `C++17`。
- 保留 `-pthread` 作为编译和链接选项，因为在 Linux/GCC/Clang 下即使改用 `std::thread`，底层线程支持和 TLS 相关能力仍依赖该选项。
- 这一周只统一编译基线，不在 P0 阶段直接重写压力测试线程模型。

### 本周完成标准
- `skiplist.h` 不再包含任何跨实例共享的全局可变状态。
- 核心接口无强依赖标准输出。
- 项目仍能跑 demo，且 P0 测试全部通过。

## Week 3：P1 并发 + API + Scan + Benchmark 改造

### 目标
本周把项目从“能跑”推进到“像一个存储组件”。重点是：API 更合理、重复 key 语义明确、读写并发模型说得清、benchmark 不再自欺欺人。

### 实现步骤

1. 升级并发模型
- 把类成员锁升级为 `mutable std::shared_mutex mutex_`。
- 读路径使用 `std::shared_lock`：
  - `get`
  - `contains`
  - `size`
  - `display_list`
  - `scan`
- 写路径使用 `std::unique_lock`：
  - `put`
  - `erase`
  - `checkpoint`
  - `recover`
- 不宣称 lock-free，不支持无锁迭代器；本轮并发模型明确为“全表级读写锁”。
- 压测与测试侧的线程模型同步切换到 `std::thread`，避免核心代码使用 C++ 锁而外围 harness 仍停留在 `pthread_create/pthread_join` 的混搭状态。

2. 重建主 API，旧 API 只做兼容层
- 新增主接口：
  - `WriteResult put(const K&, const V&)`
  - `std::optional<V> get(const K&) const`
  - `bool contains(const K&) const`
  - `bool erase(const K&)`
  - `std::vector<std::pair<K, V>> scan(const K& begin, const K& end) const`
  - `size_t size() const`
  - `bool checkpoint()`
  - `bool recover()`
- `WriteResult` 明确定义为：
  - `inserted`
  - `updated`
- `scan` 采用半开区间 `[begin, end)`；若 `begin >= end`，返回空结果。
- 保留原有兼容接口，但只作为薄包装：
  - `search_element` 调 `contains`
  - `delete_element` 调 `erase`
  - `insert_element` 维持“仅插入”语义，重复 key 返回失败，不改值
- 从第 3 周开始，`main.cpp`、测试和 benchmark 全部改用新主 API，不再依赖旧接口表达新能力。

3. 明确重复 key 策略
- 默认写入路径是 `put`，重复 key 覆盖旧值。
- 旧 `insert_element` 仅保留兼容语义，避免破坏性升级过大。
- 第 4 周 WAL 只记录 `put` / `erase`，不为“插入失败”单独设计日志类型。

4. 加入范围扫描能力
- `scan(begin, end)` 从第一个 `>= begin` 的节点开始，在 level0 顺序收集，直到 `key >= end` 停止。
- 结果按 key 升序返回。
- 本轮不做懒迭代器，不暴露指针级遍历接口，避免锁生命周期复杂化。

5. 重写 benchmark，不再沿用“打印每次插入”的压测方式
- 把现有 `stress-test/stress_test.cpp` 从 `pthread` 改为 `std::thread`：
  - 删除 `pthread.h`
  - 用普通函数或 lambda 作为线程入口，不再使用 `void*` 线程参数
  - 用 `std::vector<std::thread>` 管理线程
  - 用类型安全的整型线程下标替代当前的指针强转传参
  - 用 `join()` 做线程回收，避免 `pthread_exit`
- 这个改造的动机不是“性能更高”，而是：
  - 与 `std::mutex` / `std::shared_mutex` / `std::optional` 等 C++ API 保持一致的语言层抽象
  - 去掉 `void*` 传参、`reinterpret_cast`、手工错误码处理这类不必要的 C 风格噪音
  - 让 benchmark 代码更容易扩展成多 workload、多参数、多轮统计的 C++ harness
  - 面试时更容易把项目表述为“现代 C++ 存储项目”，而不是“C 风格线程 API + C++ 容器/锁的混合体”
- benchmark 接受命令行参数：
  - `--threads`
  - `--ops`
  - `--key-space`
  - `--workload=put|get|mixed|scan`
  - `--seed`
  - `--scan-width`
  - `--sync-wal`
- `mixed` 默认 50% `put` + 50% `get`。
- `scan` workload 默认每次扫描 `scan-width=100` 个 key 的范围。
- 每个 workload 运行 3 轮，输出：
  - 总耗时 ms
  - 吞吐 ops/s
  - 3 轮中位吞吐
- benchmark 中彻底关闭库日志。
- benchmark 默认 `sync_wal=false`，避免把 WAL durability 和内存结构吞吐混在一起。

6. 扩展 P1 测试
- `put` 首次写入返回 `inserted`，重复写入返回 `updated`。
- `get` 命中返回值，未命中返回空。
- `scan` 在空区间、单元素区间、跨层随机数据中保持有序且边界正确。
- 并发 smoke test：
  - 多线程 `put`
  - 多线程 `get`
  - 读写混合
- 兼容接口行为保持稳定。

### 本周完成标准
- 你可以明确回答“这个项目现在是如何做读写并发控制的”。
- benchmark 输出不再受每次操作日志影响。
- `put/get/erase/scan` 构成一套更像 KV 引擎的主接口。
- 压测与并发 smoke test 已全部切换到 `std::thread`，仓库代码层面不再直接依赖 `pthread` API。

## Week 4：P1 WAL + 恢复 + Checkpoint + 面试收口

### 目标
本周补上最关键的存储工程能力：基础 WAL、启动恢复和手动 checkpoint。目标是“能讲清 crash recovery 链路”，不是完整生产级容错。

### 实现步骤

1. 定义 WAL 与 snapshot 格式
- WAL 采用文本追加格式：
  - `P\t<key>\t<escaped_value>\n`
  - `D\t<key>\n`
- Snapshot 采用：
  - 第一行固定头：`SKIPLIST_SNAPSHOT_V1`
  - 后续每行：`<key>\t<escaped_value>\n`
- 统一实现 value 转义/反转义，至少处理：
  - `\t`
  - `\n`
  - `\\`

2. 定义写入顺序
- `put` / `erase` 的写路径顺序固定为：
  - 先追加 WAL
  - 若 `sync_wal=true`，执行 `flush + fsync`
  - WAL 成功后再修改内存跳表
- 若 WAL 写入失败，直接返回失败，不修改内存。
- benchmark 默认关闭 `sync_wal`；正确性测试开启 `sync_wal`。

3. 实现恢复流程
- `recover()` 固定顺序：
  - 清空当前内存表
  - 若 snapshot 存在，先完整加载 snapshot
  - 若 WAL 存在，再顺序 replay WAL
- replay 规则：
  - `P` 记录等价于 `put`
  - `D` 记录等价于 `erase`
- 基础版错误策略：
  - 遇到格式错误、非法 key、无法反转义的记录时，`recover()` 直接返回 `false`
  - 不做损坏修复、不跳过坏记录、不尝试自动截断

4. 实现 checkpoint
- `checkpoint()` 固定顺序：
  - 持有写锁，确保 checkpoint 与写入互斥
  - 以当前 level0 顺序生成新 snapshot
  - snapshot 写完并同步后，清空旧 WAL
  - 重新打开 WAL 以接收后续写入
- checkpoint 完成后，新的恢复起点为“snapshot + 空 WAL”。

5. 收口 demo、README 与面试材料
- `main.cpp` 改成演示：
  - `put`
  - `get`
  - `scan`
  - `checkpoint`
  - 新实例 `recover`
- README 只更新本轮真实落地的能力，不写未来能力冒充现状。
- 同步整理 3 类面试问答：
  - 为什么跳表适合做 MemTable
  - 为什么需要 WAL + checkpoint
  - 为什么本轮只做到基础恢复，不做 SSTable/compaction

6. 扩展恢复测试
- WAL-only 恢复：无 snapshot，只有若干 `put/delete` 日志。
- snapshot + WAL 恢复：先 checkpoint，再继续写，再重启恢复。
- 覆盖旧值恢复：同一个 key 多次 `put` 后恢复到最终值。
- 删除恢复：`put` 后 `erase`，恢复后应不存在。
- 格式错误恢复：坏 WAL / 坏 snapshot 返回失败。
- 兼容性测试：禁用 WAL 时仍可使用纯内存模式。

### 本周完成标准
- 你能画出“写入 -> WAL -> 内存 -> checkpoint -> recover”的完整链路。
- 项目可以在新实例中通过 `recover()` 重建状态。
- 你能明确说明 P1 的恢复能力边界：基础可恢复，但还不是生产级 crash consistency。

## Public APIs / Types

本轮实施完成后，主接口与行为固定如下：

- `SkipListOptions`
  - `max_level`
  - `snapshot_path`
  - `wal_path`
  - `enable_wal`
  - `sync_wal`
  - `enable_debug_output`

- `WriteResult`
  - `inserted`
  - `updated`

- 主 API
  - `put(const K&, const V&) -> WriteResult`
  - `get(const K&) const -> std::optional<V>`
  - `contains(const K&) const -> bool`
  - `erase(const K&) -> bool`
  - `scan(const K& begin, const K& end) const -> std::vector<std::pair<K, V>>`
  - `size() const -> size_t`
  - `checkpoint() -> bool`
  - `recover() -> bool`

- 兼容 API
  - 保留 `insert_element/search_element/delete_element/dump_file/load_file`
  - 只作为过渡包装，不再作为文档主接口

## Test Plan

必须覆盖的测试场景：
- 基础功能：插入、覆盖、删除、查询、空表、边界 key
- 顺序性质：level0 全局有序，`scan` 结果有序
- 兼容性：旧接口可用，新接口是主路径
- 并发 smoke：多线程读、多线程写、混合读写不崩溃且结果满足最终一致
- 持久化：snapshot round-trip、WAL-only recover、snapshot+WAL recover
- 失败路径：非法 snapshot、非法 WAL、WAL 写入失败时内存不应被更新
- 资源释放：大数据量析构不因递归清理导致栈问题
- benchmark smoke：不同 workload 参数可执行并输出统计摘要

## P2 Preview

P2 不进入本轮实现，只保留下一阶段路线：
- 把当前 MemTable + WAL 演进为 `MemTable + Immutable MemTable + SSTable`
- 加入后台 flush 和基础 compaction
- 为 SSTable 增加块索引和 Bloom Filter
- 把错误处理从“fail fast”升级到“checksum + 损坏检测 + 部分恢复策略”
- 把轻量测试升级为 `GoogleTest + CI`
- benchmark 再升级到更系统的延迟统计和多负载报告

## Assumptions

- 平台以 Linux/WSL 为默认环境，可接受 `fsync` 方案
- 本轮不追求完全跨平台文件持久化抽象
- 4 周内不做大规模目录拆分，优先保住功能改造成果
- 持久化能力只对 `int -> std::string` 的 KV 对外承诺
- `put` 是主写接口，重复 key 默认覆盖旧值
- `scan` 使用半开区间 `[begin, end)`
- benchmark 的目标是“可信比较”，不是发布绝对权威性能数字
- `pthread` 只作为编译/链接层依赖保留在 `-pthread` 选项中，不再作为代码层线程 API 使用
