# Week 2 工程约束 V2

## 1. 目的

这份约束用于冻结 `Week 2` 完成后的工程边界，避免后续在进入 `Week 3` 之前重新引入 P0 已经解决的问题。

它和 [Week 1 工程约束 V1](../week1/engineering-constraints-v1.md) 的关系是：

1. `V1` 约束的是“先搭最小测试和基线”。
2. `V2` 约束的是“P0 工程化改造已经落地后的代码边界”。

## 2. 当前阶段的稳定前提

进入 `Week 3` 前，默认接受下面这些已经落地的前提：

1. `skiplist.h` 不再依赖头文件全局可变状态。
2. `SkipListOptions` 是新的实例级配置入口。
3. 核心接口不再依赖 `std::cout` 作为行为表达方式。
4. 基线测试和 P0 测试已经是默认回归护栏。
5. 构建基线统一为 `C++17 + -pthread`。

## 3. Week 2 之后新增约束

### 3.1 配置入口约束

1. 新增配置项时，优先收敛到 `SkipListOptions`，不要再引回新的全局宏或头文件全局状态。
2. snapshot 路径必须通过 `SkipListOptions::store_file` 或默认构造路径进入实例，不再允许在逻辑里硬编码新路径分支。
3. `wal_path`、`enable_wal`、`sync_wal`、`enable_debug_output` 目前可以继续作为占位配置，但后续新增能力必须优先复用这些字段，而不是重新发明一套平行配置。

### 3.2 输出与接口约束

1. `display_list` 的默认输出仍允许是 `std::cout`，但实现必须继续保持“写入传入流”的能力。
2. 不要重新把 `insert_element`、`search_element`、`delete_element`、`dump_file`、`load_file` 改回依赖打印日志的模式。
3. 后续如果新增调试输出，应通过配置开关或调用方传入流来控制，而不是在核心路径直接 `std::cout`。

### 3.3 资源释放约束

1. 节点销毁必须继续沿迭代清理路径走，不允许回退到递归 `clear`。
2. 任何后续清理逻辑调整，都必须保持 `test_iterative_destructor_stress` 持续通过。

### 3.4 持久化边界约束

1. `dump_file` 和 `load_file` 当前只对 `SkipList<int, std::string>` 开放，这是代码层面的显式约束，不允许在文档或提交说明里把当前实现表述成“完全泛型持久化”。
2. 后续如果要放开持久化类型支持，必须同步改 trait、解析逻辑和测试，不能只去掉 `static_assert`。
3. 当前自定义 snapshot 路径已经进入回归测试，后续改动不能破坏这个能力。

### 3.5 测试体系约束

1. `tests/test_main.cpp` 仍然是当前默认的单二进制回归入口。
2. 新增 P0/P1 测试时，优先在现有框架内扩展，不在没有必要的情况下拆成多个测试可执行文件。
3. 任何会影响 `store_file` 行为的修改，都必须至少跑：
   - 自定义路径 round-trip
   - 双实例路径隔离
4. 任何会影响展示逻辑的修改，都必须保住“写入传入流，而非偷偷写 stdout”的语义。

### 3.6 构建与基线约束

1. `makefile`、`compile_flags.txt`、`stress_test_start.sh` 的编译标准必须继续保持一致。
2. 后续线程模型即使切到 `std::thread`，也应继续保留 `-pthread` 基线。
3. `Week 3` 之前，不把 benchmark 从 `pthread` 强行半改到一半；要么保持当前基线，要么在 `Week 3` 整体切换。

## 4. 当前阶段明确不做的事情

在进入 `Week 3` 之前，下面这些事情仍然不属于 `Week 2` 完成态：

1. 不引入 `put/get/erase/scan` 新主 API
2. 不重构为完整读写锁语义
3. 不引入 WAL 恢复链路
4. 不把 benchmark 重写成参数化多 workload harness
5. 不把持久化格式升级成带版本号和 checksum 的协议

## 5. Week 2 完成后的最小回归命令

进入下一阶段前，建议至少执行：

```bash
make test
make
make stress
```

如果这里有任意一步失败，就不应继续向 `Week 3` 推进。

## 6. 一句话结论

`Week 2` 之后的代码基线，不再是“能跑的教程代码”，而是“已经具备实例级配置、测试护栏、可控输出和基础工程边界的 P0 版本”。后续所有改造都应该建立在这个基线上，而不是绕开它。
