# Skiplist-CPP 项目结构概览

## 1. 一句话定位

这个仓库本质上是一个基于跳表的轻量级 KV Demo。

它已经实现了：

1. 内存中的跳表增删查
2. 第 0 层有序遍历
3. 基于文本文件的全量 dump / load
4. 一个 demo 入口和一个简单压测入口

它还没有实现：

1. WAL
2. 崩溃一致性恢复
3. 多版本或多层存储
4. 严谨的并发模型
5. 可信的 benchmark 体系

## 2. 代码结构

```text
Skiplist-CPP/
├── skiplist.h                     核心实现，几乎所有逻辑都在这里
├── main.cpp                       demo 程序，演示插入/查询/删除/落盘
├── stress-test/
│   └── stress_test.cpp            压测程序，当前主要测插入
├── stress_test_start.sh           编译并运行压测
├── store/
│   └── dumpFile                   当前持久化输出文件
├── bin/                           生成的可执行文件目录
├── README.md                      中文说明
├── README-en.md                   英文说明
└── docs/                          阅读、缺陷、改造和面试材料
```

## 3. 核心模块职责

### 3.1 [skiplist.h](../skiplist.h)

这是项目的核心。

它同时承担了：

1. 数据结构定义
2. 内存写路径
3. 内存读路径
4. 删除路径
5. 文本持久化
6. 文件加载恢复
7. 析构和资源释放

从工程拆分角度看，这意味着“一个头文件承担了几乎全部职责”。

### 3.2 [main.cpp](../main.cpp)

这个文件不是业务入口，而是一个演示程序。

它展示的主链路是：

1. 构造 `SkipList<int, std::string>`
2. 插入若干中文键值
3. 调用 `dump_file`
4. 查询存在和不存在的 key
5. 展示跳表
6. 删除两个 key
7. 再次展示跳表

所以它更像“行为样例”，不是完整的产品接口层。

### 3.3 [stress-test/stress_test.cpp](../stress-test/stress_test.cpp)

这个文件是当前唯一的 benchmark 入口，但它的定位更接近“粗糙压测脚本”。

当前主要特征：

1. 使用 `pthread`
2. 默认常量控制线程数和操作数
3. 当前主路径只测插入
4. 查询压测代码被注释掉
5. 压测结果会被大量日志污染

## 4. 运行时关系图

```text
main.cpp / stress_test.cpp
          |
          v
     SkipList<K, V>
          |
          +--> insert_element / search_element / delete_element
          |
          +--> dump_file --> store/dumpFile
          |
          +--> load_file <-- store/dumpFile
```

## 5. 数据路径视角下的结构

### 5.1 内存路径

1. `SkipList` 持有 `_header`
2. `_header` 是每层入口
3. 业务数据沿第 0 层形成完整有序链
4. 高层 `forward` 指针作为加速索引

### 5.2 持久化路径

1. `dump_file` 顺着第 0 层遍历
2. 把内存数据写到 `store/dumpFile`
3. `load_file` 再从文件逐行读回
4. 通过 `insert_element` 重建跳表

### 5.3 并发路径

1. 写路径 `insert_element` / `delete_element` 共享一把全局锁
2. 读路径 `search_element` / `display_list` 无锁
3. 持久化路径 `dump_file` / `load_file` 也没有完整并发保护

## 6. 这份结构图最该记住什么

1. 这是一个 header-only 的跳表 KV Demo，核心逻辑高度集中在 `skiplist.h`。
2. 项目对外看起来有 demo、压测和持久化，但底层实现仍然是“单文件核心 + 文本 dump”。
3. 如果你后续要做工程化改造，拆分的第一优先级通常是：并发模型、持久化模型、接口语义和测试体系。
