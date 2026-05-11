# Skiplist-CPP 当前持久化流程图

## 1. 一句话结论

当前项目的持久化模型不是 WAL，也不是 snapshot + replay，而是最简单的“全量文本 dump + 启动时逐行 load”。

## 2. 全局流程图

```mermaid
flowchart LR
    A[内存中的 SkipList] --> B[dump_file]
    B --> C[顺着 level 0 遍历]
    C --> D[写入 store/dumpFile\n格式: key:value]
    D --> E[文本快照文件]
    E --> F[load_file]
    F --> G[getline 逐行读取]
    G --> H[按第一个冒号切分 key/value]
    H --> I[stoi(key)]
    I --> J[insert_element]
    J --> K[重建内存跳表]
```

## 3. `dump_file` 细化流程

```mermaid
flowchart TD
    A[开始 dump_file] --> B[open STORE_FILE]
    B --> C[node = _header->forward[0]]
    C --> D{node != nullptr?}
    D -- 是 --> E[写出 key:value 加换行]
    E --> F[node = node->forward[0]]
    F --> D
    D -- 否 --> G[flush]
    G --> H[close]
    H --> I[结束]
```

### `dump_file` 的实际语义

1. 它写的是“当前内存状态的全量快照”。
2. 它只遍历第 0 层，因为第 0 层已经包含全部有序数据。
3. 它不会自动触发，也不会对插入和删除做增量记录。

## 4. `load_file` 细化流程

```mermaid
flowchart TD
    A[开始 load_file] --> B[open STORE_FILE]
    B --> C[new string key / value]
    C --> D[getline 读一行]
    D --> E{成功读到行?}
    E -- 否 --> F[delete key/value]
    F --> G[close]
    G --> H[结束]
    E -- 是 --> I[get_key_value_from_string]
    I --> J{key 为空 或 value 为空?}
    J -- 是 --> D
    J -- 否 --> K[stoi(key)]
    K --> L[insert_element]
    L --> D
```

### `load_file` 的实际语义

1. 它不是“清空后恢复快照”，而是“把文件里的记录继续插入当前跳表”。
2. 它要求 key 能被 `stoi` 转为 `int`，所以持久化路径是半泛型设计。
3. 它依赖 `insert_element` 重建结构，因此重复 key 会被直接忽略，而不是覆盖。

## 5. 当前流程的关键限制

1. 没有 WAL，所以两次 `dump_file` 之间的修改一旦崩溃就会丢失。
2. `dump_file` 直接覆盖正式文件，写到一半崩溃可能留下坏快照。
3. `load_file` 不清空当前内存，恢复语义不完整。
4. 文件格式是 `key:value`，没有版本号、校验和、转义规则和损坏检测。

## 6. 配套阅读

1. [当前持久化缺陷清单](./skiplist-persistence-defect-list.md)
