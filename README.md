# [English Version](./README-en.md)

# KV 存储引擎

这是一个基于跳表实现的轻量级 KV 存储引擎，当前定位是“可讲工程取舍的教学型组件”，不是完整数据库。

当前代码基线已经包含：

* C++17 header-only 跳表实现
* 新主接口：`put / get / contains / erase / scan / checkpoint / recover`
* 兼容旧接口：`insert_element / search_element / delete_element / dump_file / load_file`
* 全表级读写锁并发模型
* `snapshot + WAL` 基础恢复链路
* 参数化 benchmark harness

## 当前持久化模型

当前落地的是 P1 级基础恢复能力：

* Snapshot 格式
  * 第一行：`SKIPLIST_SNAPSHOT_V1`
  * 后续每行：`<key>\t<escaped_value>`
* WAL 格式
  * `P\t<key>\t<escaped_value>`
  * `D\t<key>`
* `recover()` 顺序
  * 先加载 snapshot
  * 再顺序回放 WAL
* `checkpoint()` 语义
  * 生成新 snapshot
  * 清空旧 WAL

当前边界也需要明确：

* 持久化只支持 `K=int, V=std::string`
* 这是基础可恢复版，不是生产级 crash consistency
* 还没有 SSTable、compaction、Bloom Filter、checksum、损坏修复

## 项目结构

* `skiplist.h`
  * 跳表核心实现
* `main.cpp`
  * 演示 `put / get / scan / checkpoint / recover`
* `tests/test_main.cpp`
  * 轻量回归测试入口
* `stress-test/stress_test.cpp`
  * benchmark harness
* `stress_test_start.sh`
  * benchmark 构建与运行脚本
* `store/`
  * 默认 snapshot/WAL 目录
* `bin/`
  * 生成的可执行文件

## 主接口

* `WriteResult put(const K&, const V&)`
* `std::optional<V> get(const K&) const`
* `bool contains(const K&) const`
* `bool erase(const K&)`
* `std::vector<std::pair<K, V>> scan(const K&, const K&) const`
* `size_t size() const`
* `bool checkpoint()`
* `bool recover()`

## 构建与测试

```bash
make
./bin/main
```

```bash
make test
```

```bash
sh stress_test_start.sh
```

也可以给 benchmark 传参，例如：

```bash
sh stress_test_start.sh --threads=4 --ops=100000 --workload=mixed --sync-wal=false
```

## Demo 行为

当前 `main.cpp` 会演示下面这条链路：

1. `put`
2. `get`
3. `scan`
4. `checkpoint`
5. checkpoint 后继续写入
6. 新实例 `recover`

## 当前测试覆盖

回归测试已经覆盖：

* 基础增删查与旧接口兼容
* `scan` 有序性
* snapshot round-trip
* WAL 写入顺序与失败路径
* WAL-only recover
* snapshot + WAL recover
* `checkpoint` 后清空 WAL
* 非法 snapshot / 非法 WAL 返回失败
* 并发 smoke test

## 后续方向

如果继续往数据库方向演进，下一阶段通常会是：

* `MemTable + Immutable MemTable + SSTable`
* 后台 flush
* compaction
* checksum 与损坏检测
* 更完整的自动化测试和 CI
