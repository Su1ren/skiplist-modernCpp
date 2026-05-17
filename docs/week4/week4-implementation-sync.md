# Week 4 改造同步记录

## 1. 目的

这份文档用于把 `docs/skiplist-cpp-4week-implementation-plan.md` 中 `Week 4` 的计划项，与当前仓库里的实际实现同步起来。

重点回答：

1. `Step 1` 到 `Step 6` 当前是否已落地。
2. `Week 4` 的完成标准是否已经满足。
3. 哪些边界已经可以作为下一阶段的稳定基线继续使用。

## 2. 完成标准对照

`Week 4` 的完成标准是：

1. 你能画出“写入 -> WAL -> 内存 -> checkpoint -> recover”的完整链路。
2. 项目可以在新实例中通过 `recover()` 重建状态。
3. 你能明确说明 P1 的恢复能力边界：基础可恢复，但还不是生产级 crash consistency。

当前状态：

1. 写路径已经固定为 `WAL -> 内存`，对应 [put](../../skiplist.h:871)、[erase](../../skiplist.h:721) 和兼容接口 [insert_element](../../skiplist.h:324)。
2. 恢复路径已经固定为 `snapshot -> WAL replay`，对应 [recover](../../skiplist.h:814) 和 [replay_wal_unlocked](../../skiplist.h:1117)。
3. `checkpoint()` 已经具备“写 snapshot、同步 snapshot、清空 WAL”的语义，见 [checkpoint](../../skiplist.h:766)。
4. README、demo 和面试材料都已经同步到当前真实能力，见：
   - [main.cpp](../../main.cpp:17)
   - [README.md](../../README.md:1)
   - [README-en.md](../../README-en.md:1)
   - [interview-qa.md](./interview-qa.md:1)

结论：

1. 按当前计划定义，`Week 4` 已达到完成状态。

## 3. Step 1 到 Step 6 的实际改造内容

### 3.1 Step 1：定义 WAL 与 snapshot 格式

当前落地结果：

1. Snapshot 头固定为 `SKIPLIST_SNAPSHOT_V1`，见 [skiplist.h](../../skiplist.h:217)。
2. Snapshot 记录格式为 `<key>\t<escaped_value>`，编码逻辑见 [encode_snapshot_line](../../skiplist.h:964)。
3. WAL 记录格式为：
   - `P\t<key>\t<escaped_value>`，见 [encode_wal_put_record](../../skiplist.h:992)
   - `D\t<key>`，见 [encode_wal_delete_record](../../skiplist.h:1003)
4. `value` 的转义/反转义已经支持：
   - `\\`
   - `\t`
   - `\n`
   见 [escape_value](../../skiplist.h:556) 和 [unescape_value](../../skiplist.h:585)。

测试覆盖：

1. [test_dump_file_writes_snapshot_header](../../tests/test_main.cpp:207)
2. [test_dump_and_load_round_trip_with_escaped_values](../../tests/test_main.cpp:219)

### 3.2 Step 2：定义写入顺序

当前落地结果：

1. `put` 已固定为“先追加 WAL，再改内存”，见 [put](../../skiplist.h:871)。
2. `erase` 已固定为“先追加 WAL，再改内存”，见 [erase](../../skiplist.h:721)。
3. 兼容接口 `insert_element` 也已经接入同样的 WAL 顺序，见 [insert_element](../../skiplist.h:324)。
4. WAL 写入实现使用 `open/write/fsync/close` 路径，见 [append_wal_line_unlocked](../../skiplist.h:1038)。

测试覆盖：

1. [test_put_with_wal_appends_put_records](../../tests/test_main.cpp:243)
2. [test_erase_with_wal_appends_delete_record](../../tests/test_main.cpp:267)
3. [test_insert_element_with_wal_logs_only_successful_insert](../../tests/test_main.cpp:293)
4. [test_put_wal_failure_does_not_modify_memory](../../tests/test_main.cpp:622)
5. [test_erase_wal_failure_does_not_modify_memory](../../tests/test_main.cpp:645)

### 3.3 Step 3：实现恢复流程

当前落地结果：

1. `recover()` 顺序已调整为：
   - 清空当前内存表
   - 若 snapshot 存在，先加载 snapshot
   - 若 WAL 存在，再顺序 replay WAL
2. WAL replay 已对 `P` 记录的 value 做反转义，见 [replay_wal_unlocked](../../skiplist.h:1117)。
3. 格式错误策略已保持 `fail fast`：
   - 坏 snapshot 返回 `false`
   - 坏 WAL 返回 `false`

测试覆盖：

1. [test_recover_returns_false_on_invalid_snapshot](../../tests/test_main.cpp:385)
2. [test_recover_from_snapshot_only_rebuilds_state](../../tests/test_main.cpp:411)
3. [test_recover_from_wal_only_replays_final_state](../../tests/test_main.cpp:461)
4. [test_recover_multiple_puts_keep_latest_value](../../tests/test_main.cpp:487)
5. [test_recover_after_put_then_erase_keeps_key_absent](../../tests/test_main.cpp:509)
6. [test_recover_from_snapshot_and_wal_replays_both](../../tests/test_main.cpp:530)
7. [test_recover_returns_false_on_invalid_wal](../../tests/test_main.cpp:571)

### 3.4 Step 4：实现 checkpoint

当前落地结果：

1. `checkpoint()` 已在写锁内完成 snapshot 落盘。
2. snapshot 落盘后会显式 `fsync`。
3. 之后会把 WAL 截断为一个空文件，并同步新 WAL 状态。
4. checkpoint 完成后的恢复起点已经是“snapshot + 空 WAL”。

测试覆盖：

1. [test_checkpoint_clears_wal_and_rebuilds_from_snapshot](../../tests/test_main.cpp:435)
2. [test_recover_from_snapshot_and_wal_replays_both](../../tests/test_main.cpp:530)

### 3.5 Step 5：收口 demo、README 与面试材料

当前落地结果：

1. [main.cpp](../../main.cpp:17) 已改成演示：
   - `put`
   - `get`
   - `scan`
   - `checkpoint`
   - 新实例 `recover`
2. [README.md](../../README.md:1) 已更新为当前真实能力描述。
3. [README-en.md](../../README-en.md:1) 已同步英文说明。
4. 已新增面试材料 [interview-qa.md](./interview-qa.md:1)，覆盖：
   - 为什么跳表适合做 MemTable
   - 为什么需要 WAL + checkpoint
   - 为什么当前只做到基础恢复

### 3.6 Step 6：扩展恢复测试

当前落地结果：

`tests/test_main.cpp` 已覆盖计划里列出的恢复测试清单：

1. WAL-only recover
   - [test_recover_from_wal_only_replays_final_state](../../tests/test_main.cpp:461)
2. snapshot + WAL recover
   - [test_recover_from_snapshot_and_wal_replays_both](../../tests/test_main.cpp:530)
3. 同 key 多次 `put` 后恢复最终值
   - [test_recover_multiple_puts_keep_latest_value](../../tests/test_main.cpp:487)
4. `put` 后 `erase`，恢复后应不存在
   - [test_recover_after_put_then_erase_keeps_key_absent](../../tests/test_main.cpp:509)
5. 坏 WAL / 坏 snapshot 返回失败
   - [test_recover_returns_false_on_invalid_snapshot](../../tests/test_main.cpp:385)
   - [test_recover_returns_false_on_invalid_wal](../../tests/test_main.cpp:571)
6. 禁用 WAL 时仍可使用纯内存模式
   - [test_disable_wal_still_supports_pure_memory_mode](../../tests/test_main.cpp:599)

## 4. 当前 Week 4 之后的代码状态

从工程形态看，当前仓库已经从“只有全量 dump 的跳表 Demo”升级到：

1. 有明确的 `snapshot + WAL` 文本格式
2. 有固定的 `WAL -> 内存` 写路径
3. 有固定的 `snapshot -> WAL replay` 恢复路径
4. 有基础 `checkpoint` 语义
5. 有 demo、README 和面试材料收口
6. 有覆盖恢复链路的默认回归测试集

但还没有进入下一阶段的内容：

1. 还没有 SSTable
2. 还没有 compaction
3. 还没有 checksum 与损坏修复策略
4. 还没有生产级 crash consistency 保证

## 5. 本周验证命令

本轮同步时实际执行并通过的命令：

```bash
make test
make
./bin/main
ASAN_OPTIONS=detect_leaks=0 g++ tests/test_main.cpp -o /tmp/skiplist_tests_asan_step6 -std=c++17 -I. -pthread -fsanitize=address -g && ASAN_OPTIONS=detect_leaks=0 /tmp/skiplist_tests_asan_step6
```

验证结果：

1. `make test`：`35/35 passed`
2. `make`：通过
3. `./bin/main`：通过，能演示 `put/get/scan/checkpoint/recover`
4. ASan：通过

## 6. 和下一阶段的衔接

如果继续推进后续版本，建议把当前 `Week 4` 状态作为新的稳定基线：

1. 保留当前 snapshot/WAL 格式
2. 保留当前 recover/checkpoint 语义
3. 保留当前恢复测试集
4. 在这个基线上继续做更高层的存储结构演进

配套约束见：

1. [Week 4 工程约束 V4](./engineering-constraints-v4.md)
