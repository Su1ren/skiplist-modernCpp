# Skiplist-CPP 当前持久化缺陷清单

## 1. 文档目的

这份文档基于当前 `skiplist.h` 的实现，总结项目现阶段与持久化相关的主要缺陷。

用途有两个：

1. 作为阅读当前代码后的问题清单，帮助你快速定位真实短板。
2. 作为 [skiplist-cpp-interview-study-plan.md](/home/suiren/Skiplist-CPP/docs/skiplist-cpp-interview-study-plan.md:1) 中“持久化恢复改造”阶段的输入材料。

配套材料：

1. [当前持久化流程图](./skiplist-persistence-flow.md)

## 2. 当前持久化模型

当前项目的持久化不是 WAL，也不是 snapshot + replay 的组合，而是最简单的全量文本落盘模型：

1. `dump_file` 顺着第 0 层遍历跳表。
2. 把每个节点按 `key:value` 格式写入 `store/dumpFile`。
3. `load_file` 启动时逐行读取文件。
4. 用 `:` 切分 key 和 value。
5. 把 key 通过 `stoi` 转成 `int`，再调用 `insert_element` 重新构建内存跳表。

这个模型能演示“内存数据可以落盘并重新加载”，但离真正可用的存储恢复链路还有明显差距。

## 3. 持久化缺陷清单

下面按严重度排序。

### 3.1 P0：`dump_file` 直接覆盖正式文件，写入过程不具备原子性

代码位置：

1. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:299)

现状：

1. `dump_file` 直接 `open(STORE_FILE)` 写正式文件。
2. 没有临时文件。
3. 没有原子 `rename`。
4. 没有 `fsync` 或等价的持久化完成保证。
5. 没有文件级 checksum 或完整性校验。

问题：

1. 进程在写文件过程中崩溃，`store/dumpFile` 可能只写了一部分。
2. 更糟的情况是文件先被截断，再只写入了前几行，恢复时会把“损坏快照”当成合法输入。

影响：

1. 无法保证快照落盘后的完整性。
2. 面试里一旦被问到“写一半宕机怎么办”，当前实现答不圆。

### 3.2 P0：没有 WAL，也没有自动持久化，最近修改很容易丢失

代码位置：

1. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:186)
2. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:395)
3. [main.cpp](/home/suiren/Skiplist-CPP/main.cpp:27)

现状：

1. `insert_element` 和 `delete_element` 只修改内存。
2. 真正落盘依赖外部显式调用 `dump_file`。
3. 当前工程里也没有后台周期 flush 机制。

问题：

1. 任意一次 `dump_file` 之后到下一次 `dump_file` 之前的修改，进程崩溃后都会丢失。
2. 这不是“恢复最近状态”，而是“恢复上一次快照状态”。

影响：

1. 数据丢失窗口很大。
2. 当前持久化更接近 demo 级备份，不是存储系统里的 crash recovery。

### 3.3 P0：`dump_file` 与内存修改并发时，快照可能不一致

代码位置：

1. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:299)
2. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:186)
3. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:395)

现状：

1. `insert_element` 和 `delete_element` 通过全局 `mtx` 保护修改路径。
2. `dump_file` 遍历第 0 层写文件时没有加锁。

问题：

1. 如果 `dump_file` 执行期间有其他线程插入或删除节点，落盘内容可能混入前后两个时刻的数据。
2. 遍历时如果链表指针被并发修改，理论上还可能触发未定义行为。

影响：

1. 即使文件写完，也不代表它是一个一致的快照。
2. 这会直接削弱“可恢复性”，因为恢复依据本身可能不正确。

### 3.4 P0：`load_file` 不是“恢复快照”，而是“把文件继续插入当前内存”

代码位置：

1. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:324)

现状：

1. `load_file` 开始前不会清空现有跳表。
2. 它只是逐行读取文件，然后调用 `insert_element`。

问题：

1. 如果对同一个实例重复调用 `load_file`，文件中不存在但内存里已有的数据不会被删除。
2. 如果快照中的某个 key 对应的 value 需要覆盖当前内存值，当前实现也做不到，因为重复 key 会被 `insert_element` 直接拒绝。

影响：

1. `load_file` 不具备“把内存恢复到磁盘状态”的语义。
2. 它更像“尝试导入一些记录”，而不是可靠恢复接口。

### 3.5 P1：坏行解析会复用上一次成功解析的 key/value

代码位置：

1. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:335)
2. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:361)

现状：

1. `load_file` 在循环外只分配了一次 `key` 和 `value`。
2. `get_key_value_from_string` 遇到非法字符串时直接 `return`。
3. 失败路径不会清空 `key` 和 `value`。

问题：

1. 如果某一行非法，而上一行曾成功解析，`key` 和 `value` 可能继续保留旧内容。
2. 后续 `if (key->empty() || value->empty())` 无法识别这种脏状态。
3. 结果可能出现重复插入尝试，或者调试日志误导排查。

影响：

1. 容错逻辑不可靠。
2. 恢复过程中遇到脏数据时，行为不可预测。

### 3.6 P1：空 value 无法正确恢复

代码位置：

1. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:340)

现状：

1. 当前判断逻辑是 `key->empty() || value->empty()` 就跳过该行。

问题：

1. 如果业务允许空字符串 value，例如 `1:`，那么这是一条合法记录。
2. 当前实现会在恢复阶段把它静默丢弃。

影响：

1. 持久化不能保证数据原样 round-trip。
2. 即便落盘成功，恢复后数据也可能变少。

### 3.7 P1：模板接口表面泛型，恢复路径实际把 key 写死成 `int`

代码位置：

1. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:346)
2. [main.cpp](/home/suiren/Skiplist-CPP/main.cpp:14)

现状：

1. 类模板声明是 `SkipList<K, V>`。
2. 但 `load_file` 里直接对 key 调用 `stoi`。

问题：

1. 只要 `K` 不是 `int` 或可无损映射到 `int`，恢复路径就不成立。
2. 这说明当前持久化协议没有真正与模板类型系统对齐。

影响：

1. 接口抽象和实现语义不一致。
2. 面试里如果宣称“这是泛型跳表存储”，很容易被追问击穿。

### 3.8 P1：缺少 I/O 错误处理，文件打开和写入失败会被静默吞掉

代码位置：

1. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:308)
2. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:330)
3. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:318)

现状：

1. `open` 后不检查 `is_open` 或 `fail`。
2. 写入、`flush`、读取结束后也不检查错误状态。

问题：

1. 文件不存在、目录缺失、权限不足、磁盘写满等情况都没有显式报错路径。
2. 调用方无法知道这次持久化到底成功了没有。

影响：

1. 落盘成功与否不可观测。
2. 故障时只能依赖人工排查，不具备基本工程可维护性。

### 3.9 P1：文本格式过于脆弱，没有版本、校验和转义规则

代码位置：

1. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:313)
2. [skiplist.h](/home/suiren/Skiplist-CPP/skiplist.h:374)

现状：

1. 当前格式是最简单的 `key:value`。
2. 分隔逻辑依赖第一个 `:`。
3. 文件里没有版本号、magic number、checksum、长度字段或 schema 信息。

问题：

1. 文件被截断或混入脏数据时，系统无法判断这是不是一份完整快照。
2. 文本协议没有正式转义规则，对复杂 value、换行、跨版本演进都不友好。

影响：

1. 格式兼容性差。
2. 后续很难平滑升级成更正规的存储文件。

## 4. 和学习计划的对应关系

这份缺陷清单与 [skiplist-cpp-interview-study-plan.md](/home/suiren/Skiplist-CPP/docs/skiplist-cpp-interview-study-plan.md:1) 的对应关系如下：

1. `3.1` 到 `3.4` 属于持久化正确性的核心问题，优先级应视为 P0。
2. `3.5` 到 `3.9` 属于恢复健壮性、类型抽象和协议设计问题，优先级可视为 P1。
3. 如果继续改造，建议优先落实学习计划中“阶段 4：做 P1 级持久化改造”的 WAL、snapshot、checksum 和恢复流程设计。

## 5. 一句话结论

当前项目已经具备“把跳表内容写到文件并重新加载”的演示能力，但它还不具备工程意义上的崩溃恢复、一致快照和可靠持久化能力。
