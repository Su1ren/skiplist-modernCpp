# Week 4 工程约束 V4

## 1. 目的

这份约束用于冻结 `Week 4` 完成后的工程边界，避免后续继续迭代时重新引入已经收敛好的持久化格式、恢复顺序和 checkpoint 语义问题。

它与前几版约束的关系是：

1. `V1` 约束最小测试骨架和基线回归。
2. `V2` 约束 P0 工程化改造后的边界。
3. `V3` 约束 P1 并发、API、scan 和 benchmark 的稳定基线。
4. `V4` 约束 `snapshot + WAL + recover + checkpoint` 已落地后的代码边界。

## 2. 当前阶段的稳定前提

进入 `Week 4` 完成态后，默认接受下面这些已经落地的前提：

1. 并发模型仍然是“全表级读写锁”。
2. `put/get/contains/erase/scan/checkpoint/recover` 是新的主接口。
3. `insert_element/search_element/delete_element/dump_file/load_file` 仍保留为兼容层。
4. 基础恢复链路已经固定为 `snapshot + WAL replay`。
5. benchmark 仍然是参数化 `std::thread` harness，而不是新的存储层抽象。

## 3. Week 4 之后新增约束

### 3.1 持久化格式约束

1. Snapshot 头固定为 `SKIPLIST_SNAPSHOT_V1`。
2. Snapshot 记录格式固定为 `<key>\t<escaped_value>`。
3. WAL 记录格式固定为：
   - `P\t<key>\t<escaped_value>`
   - `D\t<key>`
4. `value` 的转义/反转义至少继续支持：
   - `\\`
   - `\t`
   - `\n`
5. 当前持久化格式只对 `K=int, V=std::string` 生效，不把它描述成完全泛型持久化。

### 3.2 写路径语义约束

1. `put`、`erase` 和兼容接口 `insert_element` 的持久化顺序固定为“先 WAL，后内存”。
2. WAL 写入失败时：
   - `put` / `insert_element` 当前通过异常中止写入
   - `erase` 返回 `false`
3. 无论接口表现形式是异常还是返回值，语义都必须保持为“WAL 失败时内存不修改”。
4. `put_unlocked` 和 `erase_unlocked` 继续只做内存结构修改，不得在内部再写 WAL。

### 3.3 恢复流程约束

1. `recover()` 的固定顺序为：
   - 清空当前内存表
   - 若 snapshot 存在，先完整加载 snapshot
   - 若 WAL 存在，再顺序 replay WAL
2. replay 规则固定为：
   - `P` 记录等价于 `put`
   - `D` 记录等价于 `erase`
3. replay WAL 时必须对 value 做反转义，不得把转义后的文本直接写回内存。
4. 格式错误策略继续保持 `fail fast`：
   - 坏 snapshot 返回 `false`
   - 坏 WAL 返回 `false`
   - 不跳过坏记录
   - 不自动修复或截断

### 3.4 checkpoint 语义约束

1. `checkpoint()` 的固定语义为：
   - 在写锁内生成新 snapshot
   - 同步 snapshot
   - 清空旧 WAL
2. checkpoint 完成后，恢复起点固定为“snapshot + 空 WAL”。
3. 后续如果调整 checkpoint 实现方式，至少要保持对外语义不变，而不是只保留“写了个 dump 文件”。

### 3.5 测试体系约束

1. `tests/test_main.cpp` 仍是默认单二进制回归入口。
2. 与 `Week 4` 直接相关的恢复测试不得回删，至少包括：
   - snapshot-only recover
   - WAL-only recover
   - snapshot + WAL recover
   - 多次 `put` 后恢复最终值
   - `put` 后 `erase` 的删除恢复
   - 坏 snapshot / 坏 WAL 返回失败
   - checkpoint 后 WAL 清空
   - 关闭 WAL 的纯内存兼容模式
3. 如果改动 `recover()`、`checkpoint()`、WAL 格式或 snapshot 格式，必须先跑完整回归。

## 4. 当前阶段明确不做的事情

在 `Week 4` 完成态下，下面这些事情仍不属于当前版本：

1. 不引入 SSTable
2. 不引入 compaction
3. 不引入 Bloom Filter
4. 不引入 checksum 与损坏修复策略
5. 不宣称当前实现具备生产级 crash consistency

## 5. Week 4 完成后的最小回归命令

进入下一阶段前，建议至少执行：

```bash
make test
make
./bin/main
sh stress_test_start.sh --threads=2 --ops=1000 --workload=mixed
```

如果这里任意一步失败，就不应继续推进更高层的存储改造。

## 6. 一句话结论

`Week 4` 之后的代码基线，已经不是“只有全量 dump 的跳表 Demo”，而是“具备基础 `snapshot + WAL + checkpoint + recover` 链路的轻量 KV 引擎”。但它仍然是教学型、基础可恢复版，而不是完整数据库。
