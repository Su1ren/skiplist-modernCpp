# Week 1 工程约束 V1

## 1. 目的

这份约束用于冻结 `Week 1` 的实施边界，避免从“补最小测试骨架”直接滑向大规模重构。

适用范围：

1. `tests/`
2. `makefile`
3. `main.cpp`
4. `skiplist.h`
5. `stress-test/stress_test.cpp`

## 2. 当前阶段的核心目标

`Week 1` 只做三件事：

1. 建立最小测试骨架
2. 补齐基线回归测试
3. 冻结当前主路径行为，给后续 P0/P1 改造提供回归基线

## 3. 工程红线

### 3.1 接口边界

1. `Week 1` 的测试对象只包含当前已有接口：`insert_element`、`delete_element`、`search_element`、`dump_file`、`load_file`、`size`。
2. `Week 1` 不提前引入 `put/get/erase/scan` 这一类新接口。
3. `main.cpp` 的 demo 行为必须继续可编译、可运行。

### 3.2 语义冻结

1. `insert_element` 的当前语义先冻结：重复 key 返回失败，不覆盖旧值。
2. `load_file` 的测试只覆盖“全新实例 + 基本 round-trip”，不把它的已知缺陷固化成长期兼容行为。
3. 测试冻结主路径正确性，不冻结明显设计缺陷。

### 3.3 类型与持久化边界

1. 内存结构继续保持 `SkipList<K, V>` 模板形式。
2. `Week 1` 持久化测试只覆盖 `K=int`、`V=std::string`。
3. 原因是当前 `load_file()` 内部依赖 `stoi`，并不是真正的全泛型恢复路径。

### 3.4 测试执行边界

1. `Week 1` 采用单测试二进制方案。
2. 所有测试串行执行，不做并行调度。
3. 由于当前持久化文件硬编码为 `store/dumpFile`，每个相关 case 运行前都必须清理文件状态。
4. 测试不对 `std::cout` 具体文案做强绑定。

### 3.5 构建系统边界

1. 允许在 `makefile` 中新增最小 `test` target。
2. 允许统一编译标准到 `C++17`。
3. `Week 1` 不做大规模构建系统重构。
4. `Week 1` 不引入第三方测试框架。

### 3.6 明确不做的事情

1. 不引入 WAL、checkpoint、SSTable、compaction、Bloom Filter。
2. 不重写 `stress_test.cpp` 的 benchmark 模型。
3. 不为了测试先重构并发模型。
4. 不为了测试先重构持久化格式。

## 4. 第一版基线测试应覆盖什么

必须覆盖：

1. 空表查询和空表删除 smoke test
2. 唯一 key 插入与 `size`
3. 重复 key 插入的旧语义
4. 删除存在 key 与删除不存在 key
5. `dump_file + load_file` 的基本 round-trip
6. 通过 dump 文件验证 level 0 有序性

可选但推荐覆盖：

1. 单元素删除
2. 空 dump 文件加载

## 5. 验收标准

满足下面条件即可认为 `Week 1` 完成：

1. `make test` 可以稳定执行
2. 测试结果能清楚输出 `[PASS]` / `[FAIL]` 和 summary
3. 主路径行为被基线测试冻结
4. 后续每次改造前后都能先跑这套测试

## 6. 后续如何使用这份约束

进入 `Week 2` 之前，先检查两件事：

1. 哪些行为是 `Week 1` 故意冻结的旧语义
2. 哪些边界是 `Week 2` 和 `Week 3` 明确要突破的

一句话说，这份文档的作用不是限制改造，而是防止在没有基线的情况下盲目改造。
