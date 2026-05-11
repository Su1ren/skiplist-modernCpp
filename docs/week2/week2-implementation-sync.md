# Week 2 改造同步记录

## 1. 目的

这份文档用于把 `docs/skiplist-cpp-4week-implementation-plan.md` 中 `Week 2` 的计划项，与当前仓库里的实际实现同步起来。

它回答三个问题：

1. `Week 2` 的 8 个步骤目前实际完成了什么。
2. 哪些完成标准已经满足。
3. 哪些内容虽然进入了代码，但还只是 P0 阶段的边界版本。

## 2. 完成标准对照

`Week 2` 的完成标准是：

1. `skiplist.h` 不再包含任何跨实例共享的全局可变状态。
2. 核心接口无强依赖标准输出。
3. 项目仍能跑 demo，且 P0 测试全部通过。

当前状态：

1. `skiplist.h` 中已移除生效中的头文件全局锁与全局分隔符，锁和分隔符都已经回收到类内或实例内语义中。
2. `insert_element`、`search_element`、`delete_element`、`dump_file`、`load_file` 已去掉强依赖 `std::cout` 的行为；`display_list` 改为 [display_list](../../skiplist.h:150) 接收 `std::ostream&`。
3. 本地验证通过：
   - `make test`：`12/12 passed`
   - `make`：通过
   - `make stress`：通过

结论：

1. 按 `Week 2` 当前完成标准，仓库已经达到“计划内完成”状态。

## 3. Step 1 到 Step 8 的实际改造内容

### 3.1 Step 1：清理头文件级全局状态

当前落地结果：

1. [skiplist.h](../../skiplist.h:7) 增加了 `#pragma once`。
2. 原先的头文件全局 `std::mutex mtx` 已停止作为生效实现使用。
3. 原先的头文件全局 `std::string delimiter` 已停止作为生效实现使用。
4. 现在的锁已经收回到 [SkipList](../../skiplist.h:210) 的成员 `std::shared_mutex mtx`。
5. 分隔符已经收回到 [SkipList](../../skiplist.h:159) 的类内静态常量 `delimiter`。

当前意义：

1. 跨实例共享同一把全局锁的问题已经消失。
2. 头文件级全局可变状态不再驱动核心逻辑。

### 3.2 Step 2：引入最小配置对象

当前落地结果：

1. 已新增 [SkipListOptions](../../skiplist.h:91)。
2. 当前配置项已经包含：
   - `max_level`
   - `store_file`
   - `wal_path`
   - `enable_wal`
   - `sync_wal`
   - `enable_debug_output`
3. 已提供：
   - 默认构造
   - 仅 `max_level` 的便捷构造
   - 完整构造
4. [SkipList(const SkipListOptions&)](../../skiplist.h:570) 已接入真正初始化。
5. `_store_file` 和 `_wal_file` 已从 `options` 复制到实例成员。

当前意义：

1. 路径配置已经不再依赖宏。
2. `Week 2 / Step 7` 的“不同实例使用不同 snapshot 路径”测试已经有了真实依赖基础。

当前边界：

1. `enable_wal`、`sync_wal`、`enable_debug_output` 目前还主要是配置占位，不构成完整运行时功能。

### 3.3 Step 3：规范命名与作用域

当前落地结果：

1. 已新增命名空间 [skiplist](../../skiplist.h:21)。
2. `SkipList` 与 `Node` 保留原命名，没有做风格大改。
3. 实际文件路径已经改为通过 `_store_file` 驱动，而不是依赖 `STORE_FILE` 宏。

当前意义：

1. 实现开始具备更清晰的作用域边界。
2. 自定义路径、测试隔离、多实例路径隔离都已经可行。

备注：

1. 文件头部仍保留了被注释掉的旧宏痕迹，但它已经不是生效逻辑。

### 3.4 Step 4：去掉库内部日志依赖

当前落地结果：

1. [display_list](../../skiplist.h:150) 已改为 `display_list(std::ostream& os = std::cout) const`。
2. `insert_element`、`search_element`、`delete_element`、`dump_file`、`load_file` 不再依赖打印日志来表达成功或失败。
3. 测试中已经新增 [test_display_list_writes_to_provided_stream_only](../../tests/test_main.cpp:247)，验证展示逻辑写入传入流，而不是偷偷依赖 `stdout`。

当前意义：

1. 核心库接口更接近“由调用方决定如何输出”。
2. benchmark 不再被库内部打印自然污染。

### 3.5 Step 5：修正析构和内存清理

当前落地结果：

1. 递归 `clear(Node*)` 路径已经被停用。
2. 已新增 [clear_all_nodes](../../skiplist.h:635)。
3. 析构函数 [~SkipList](../../skiplist.h:597) 已切换到迭代释放路径。
4. 测试中已新增 [test_iterative_destructor_stress](../../tests/test_main.cpp:265)，覆盖大批量插入后的析构压力场景。

当前意义：

1. `Week 1` 中识别出的递归析构栈深风险，已经在 P0 阶段得到处理。

### 3.6 Step 6：明确“模板与持久化”的边界

当前落地结果：

1. 内存结构仍保持 `SkipList<K, V>` 模板形式。
2. 已新增 [persistence_supported](../../skiplist.h:117) trait。
3. 已新增 [is_persistence_supported_v](../../skiplist.h:126)。
4. [dump_file](../../skiplist.h:356) 和 [load_file](../../skiplist.h:379) 已通过 `static_assert` 限制为 `int + std::string` 持久化路径。
5. 构造函数中新增了最小运行时约束检查 `assert(!this->_store_file.empty())`。

当前意义：

1. “内存泛型”和“持久化受限”之间的边界已经在代码层面显式化。
2. 不再伪装成“完全泛型持久化”。

### 3.7 Step 7：扩展 P0 测试

当前落地结果：

当前测试入口在 [tests/test_main.cpp](../../tests/test_main.cpp:1)，共 `13` 个 case。

原 Week 1 基线测试：

1. `test_empty_search_and_delete_smoke`
2. `test_insert_and_size`
3. `test_duplicate_insert_keeps_old_semantics`
4. `test_delete_existing_and_missing`
5. `test_dump_and_load_round_trip`
6. `test_level0_order_via_dump_file`
7. `test_single_element_delete`
8. `test_delete_can_reduce_skip_list_levels`
9. `test_load_file_on_empty_dump_file`

Week 2 新增 P0 测试：

1. [test_custom_snapshot_path_round_trip](../../tests/test_main.cpp:191)
   - 验证自定义 snapshot 路径能正确 dump / load
2. [test_two_instances_with_different_snapshot_paths_do_not_interfere](../../tests/test_main.cpp:218)
   - 验证两个实例的持久化路径互不干扰
3. [test_display_list_writes_to_provided_stream_only](../../tests/test_main.cpp:247)
   - 验证 `display_list` 不依赖 `stdout`
4. [test_iterative_destructor_stress](../../tests/test_main.cpp:265)
   - 验证大量节点析构不再走递归清理
5. [test_delete_can_reduce_skip_list_levels](../../tests/test_main.cpp:201)
   - 通过 `display_list` 输出验证删除过程中 `_skip_list_level` 会被回收

验证结果：

1. 当前 `make test` 输出 `13/13 passed`。

### 3.8 Step 8：统一构建基线，为 P1 线程模型改造做准备

当前落地结果：

1. [makefile](../../makefile:1) 已统一为 `C++17`。
2. [compile_flags.txt](../../compile_flags.txt:1) 已统一为 `C++17 + -pthread + -I.`。
3. 测试构建和 demo 构建都使用相同基线。
4. 已新增 [make stress](../../makefile:14) 目标。
5. [stress_test_start.sh](../../stress_test_start.sh:1) 已切到 `C++17 + -I. + -pthread`。
6. 当前 benchmark 代码仍保留 `pthread` 模型，没有在 Week 2 提前重写到 `std::thread`。

当前意义：

1. `Week 3` 做线程模型与 benchmark 重构前，编译基线已经先统一。
2. 这符合计划里“Week 2 只统一基线，不直接重写 benchmark”的边界。

## 4. 当前 Week 2 之后的代码状态

从工程形态看，当前仓库已经从“教学代码直接跑起来”提升到：

1. 有实例级配置对象
2. 有命名空间
3. 有最小测试体系
4. 有可配置 snapshot 路径
5. 有迭代析构
6. 有持久化类型边界
7. 有统一构建基线

但还没有进入 `Week 3` 的内容：

1. 读写锁语义还没有完整接入读路径
2. 还没有 `put/get/erase/scan` 新主 API
3. benchmark 还没有切到 `std::thread`
4. 还没有 WAL 和恢复流程

## 5. 本周验证命令

本轮同步时实际执行并通过的命令：

```bash
make test
make
make stress
```

验证结果：

1. `make test`：`13/13 passed`
2. `make`：通过
3. `make stress`：通过

## 6. 和下一阶段的衔接

如果继续推进 `Week 3`，推荐把 `Week 2` 代码状态当作新的稳定基线：

1. 保留当前 `SkipListOptions`
2. 保留当前测试入口和回归集
3. 在此基础上引入读写锁语义、主 API 重建和 benchmark 线程模型改造

配套约束见：

1. [Week 2 工程约束 V2](./engineering-constraints-v2.md)
