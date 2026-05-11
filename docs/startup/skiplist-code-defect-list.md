# Skiplist-CPP 当前代码缺陷清单

## 1. 文档目标

这份文档不是只列持久化问题，也不是只列内存问题，而是从“当前项目整体质量”出发，汇总最重要的代码缺陷。

如果只记一件事，那就是：

1. 当前项目已经具备演示价值
2. 但它仍然更像跳表存储 Demo，而不是工程化完成的存储组件

## 2. P0 级缺陷

### 2.1 头文件中定义了全局可变状态

现状：

1. `skiplist.h` 里直接定义了全局 `mtx`
2. `skiplist.h` 里直接定义了全局 `delimiter`

影响：

1. 所有实例共享同一把锁
2. 所有实例共享同一个分隔符状态
3. 多翻译单元场景下存在 ODR 风险

### 2.2 并发模型不完整，存在数据竞争和悬空访问风险

现状：

1. 只有 `insert_element` 和 `delete_element` 加锁
2. `search_element`、`display_list`、`dump_file`、`load_file`、`size` 无锁

影响：

1. 读写并发时可能读取到不一致状态
2. 删除并发下有 use-after-free 风险
3. 不能宣称当前实现严格线程安全

### 2.3 持久化不具备可靠恢复语义

现状：

1. 只有全量文本 dump
2. 没有 WAL
3. `dump_file` 直接覆盖正式文件
4. `load_file` 不会清空现有内存

影响：

1. 崩溃时很容易丢失最近修改
2. 快照写一半时可能留下坏文件
3. `load_file` 不是严格意义上的恢复接口

配套阅读：

1. [当前持久化缺陷清单](./skiplist-persistence-defect-list.md)

### 2.4 `Node()` 默认构造函数和析构函数组合存在非法释放风险

现状：

1. `Node()` 不初始化 `forward`
2. `~Node()` 无条件 `delete[] forward`

影响：

1. 一旦有人使用默认构造的 `Node`，析构时就可能对未初始化指针执行释放

### 2.5 析构依赖递归 `clear`，大数据量时有栈溢出风险

现状：

1. `~SkipList` 通过递归 `clear` 清理第 0 层链表

影响：

1. 节点数大时，析构深度线性增长
2. 清理路径本身不稳

## 3. P1 级缺陷

### 3.1 模板接口是半泛型设计

现状：

1. `SkipList<K, V>` 看起来是泛型
2. `load_file` 却直接对 key 调用 `stoi`

影响：

1. 持久化恢复路径实际上把 key 写死成了 `int`
2. 抽象层和实现层不一致

### 3.2 `Node` 自管理裸指针，但没有禁止拷贝

现状：

1. `Node` 拥有 `forward` 的堆内存
2. 没有显式删除拷贝构造和拷贝赋值

影响：

1. 一旦按值复制，存在 double free 风险

### 3.3 `load_file` 使用手工 `new/delete` 管理临时字符串

现状：

1. `key` 和 `value` 是堆对象
2. 正常路径结尾才 `delete`

影响：

1. 风格上不符合现代 C++ RAII
2. 异常路径上可能泄漏

### 3.4 `update[_max_level + 1]` 依赖非标准 VLA 行为

现状：

1. 插入和删除都使用按运行时大小确定的栈数组

影响：

1. 标准 C++ 可移植性差
2. 大配置下也会增加栈压力

### 3.5 核心库逻辑和控制台输出耦合过深

现状：

1. `insert_element`、`search_element`、`delete_element`、`dump_file`、`load_file` 内部都直接 `std::cout`

影响：

1. 不利于作为库复用
2. 干扰 benchmark
3. 调用者很难用返回值或错误码做可靠处理

### 3.6 文件路径和持久化格式硬编码

现状：

1. `STORE_FILE` 是宏
2. 文件格式固定为 `key:value`

影响：

1. 不利于测试隔离
2. 不利于多实例配置
3. 不利于格式演进

## 4. P2 级缺陷

### 4.1 没有 include guard 或 `#pragma once`

影响：

1. 头文件自保护不足

### 4.2 没有命名空间

影响：

1. 容易和外部工程符号冲突

### 4.3 接口语义比较原始

现状：

1. 重复 key 插入直接失败，不提供更新语义
2. 查询接口只返回 `bool`
3. 没有 scan / iterator

影响：

1. 更像教学代码，不像完整 KV 组件接口

## 5. 这份缺陷清单和其他文档的关系

如果想按主题继续深挖，可以继续看：

1. [当前持久化缺陷清单](./skiplist-persistence-defect-list.md)
2. [资源生命周期笔记](./skiplist-resource-lifecycle-notes.md)
3. [内存管理风险清单](./skiplist-memory-management-risk-list.md)
4. [当前并发模型和问题清单](./skiplist-concurrency-model-and-issue-list.md)
5. [benchmark 局限性清单](./skiplist-benchmark-limitations.md)

## 6. 一句话结论

当前项目的主要缺陷不是“算法错误”，而是全局状态、并发语义、持久化可靠性、资源管理和工程抽象都还停留在 Demo 阶段。
