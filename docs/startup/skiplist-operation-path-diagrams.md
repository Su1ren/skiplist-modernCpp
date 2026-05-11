# Skiplist-CPP 插入、查询、删除路径图

## 1. 使用说明

这份文档只画当前 `skiplist.h` 的真实路径，不画理想化版本。

重点关注三件事：

1. 从哪一层开始走
2. 什么时候横向移动，什么时候下降
3. 哪些步骤会修改结构，哪些步骤只是读取

## 2. 插入路径图

对应函数：

1. `insert_element`

特征：

1. 写路径
2. 会加全局锁
3. 使用 `update[]` 保存每层前驱

```mermaid
flowchart TD
    A[开始 insert_element(key, value)] --> B[加锁 mtx.lock]
    B --> C[从 _skip_list_level 开始逐层查找]
    C --> D{当前层 next 存在且 next.key < key?}
    D -- 是 --> E[沿当前层向右移动 current]
    E --> D
    D -- 否 --> F[记录 update[i] = current]
    F --> G{还有更低层?}
    G -- 是 --> H[下降到 i - 1 层]
    H --> D
    G -- 否 --> I[移动到 level 0 的候选节点 current = current->forward[0]]
    I --> J{current 存在且 current.key == key?}
    J -- 是 --> K[返回已存在，不插入]
    K --> L[解锁并结束]
    J -- 否 --> M[生成 random_level]
    M --> N{random_level > _skip_list_level?}
    N -- 是 --> O[高层 update 补成 _header\n并提升 _skip_list_level]
    N -- 否 --> P[保持当前层高]
    O --> Q[create_node 创建新节点]
    P --> Q
    Q --> R[对 0..random_level 逐层改指针\n插入新节点]
    R --> S[_element_count++]
    S --> L
```

### 插入路径要点

1. 先找每层前驱，再一次性回接指针，这是 `update[]` 的核心作用。
2. 当前重复 key 的语义是“拒绝插入，不覆盖旧值”。
3. 这条路径的平均复杂度是 `O(log n)`，但并发上只有粗粒度串行写。

## 3. 查询路径图

对应函数：

1. `search_element`

特征：

1. 读路径
2. 不加锁
3. 不修改结构

```mermaid
flowchart TD
    A[开始 search_element(key)] --> B[current = _header]
    B --> C[从 _skip_list_level 开始逐层查找]
    C --> D{当前层 next 存在且 next.key < key?}
    D -- 是 --> E[沿当前层向右移动 current]
    E --> D
    D -- 否 --> F{还有更低层?}
    F -- 是 --> G[下降到 i - 1 层]
    G --> D
    F -- 否 --> H[到 level 0 后前进一步\ncurrent = current->forward[0]]
    H --> I{current 存在且 current.key == key?}
    I -- 是 --> J[返回 Found]
    I -- 否 --> K[返回 Not Found]
```

### 查询路径要点

1. 查询和插入一样，也是“高层横跳 + 逐层下降”。
2. 它只在最后一步检查目标节点是否命中。
3. 当前实现无锁，所以并发删除时这里有读到悬空节点的风险。

## 4. 删除路径图

对应函数：

1. `delete_element`

特征：

1. 写路径
2. 会加全局锁
3. 也依赖 `update[]`

```mermaid
flowchart TD
    A[开始 delete_element(key)] --> B[加锁 mtx.lock]
    B --> C[从 _skip_list_level 开始逐层查找前驱]
    C --> D{当前层 next 存在且 next.key < key?}
    D -- 是 --> E[沿当前层向右移动 current]
    E --> D
    D -- 否 --> F[记录 update[i] = current]
    F --> G{还有更低层?}
    G -- 是 --> H[下降到 i - 1 层]
    H --> D
    G -- 否 --> I[候选节点 current = current->forward[0]]
    I --> J{current 存在且 current.key == key?}
    J -- 否 --> K[直接解锁并结束]
    J -- 是 --> L[逐层把 update[i]->forward[i]\n绕过 current]
    L --> M[回收空高层\n递减 _skip_list_level]
    M --> N[delete current]
    N --> O[_element_count--]
    O --> K
```

### 删除路径要点

1. 删除的前半段和插入几乎一样，核心也是先找每层前驱。
2. 真正删除时先改链，再 `delete current`。
3. 删除后要收缩 `_skip_list_level`，否则会保留无元素高层。

## 5. 三条路径放在一起看

1. 插入和删除都是“先查找路径，再修改结构”，所以都需要 `update[]`。
2. 查询只读不写，但当前没有锁保护，所以不是严格线程安全。
3. 从面试表达角度，这三条路径就是你解释跳表平均 `O(log n)` 的主干材料。
