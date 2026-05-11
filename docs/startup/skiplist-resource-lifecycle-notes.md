# Skiplist-CPP 资源生命周期笔记

## 1. 文档范围

这份笔记只针对当前 `skiplist.h` 的实现，目标是回答两个问题：

1. 这个跳表实现里到底有哪些资源。
2. 每类资源是在哪里创建、由谁持有、什么时候释放的。

这里的“资源”包括：

1. 堆内存
2. 文件流
3. 互斥锁
4. 需要显式维护生命周期的临时对象

## 2. 资源地图

### 2.1 进程级全局资源

定义位置：

1. `skiplist.h:19` `std::mutex mtx`
2. `skiplist.h:20` `std::string delimiter`

生命周期特征：

1. 这两个对象不是 `SkipList` 成员，而是头文件中的全局定义。
2. 它们的生命周期从程序启动后的静态初始化开始，到进程结束时销毁。
3. 所有 `SkipList` 实例共享同一把锁和同一个分隔符字符串。

### 2.2 `Node<K, V>` 拥有的资源

定义位置：

1. `skiplist.h:30-57`

成员资源：

1. `key`
2. `value`
3. `forward`
4. `node_level`

所有权关系：

1. `key` 和 `value` 是节点对象内嵌成员，随节点对象本身一起构造和析构。
2. `forward` 是单独申请的堆数组，节点对象拥有它的唯一所有权。

创建路径：

1. `Node(K, V, int)` 在 `skiplist.h:60-74`
2. `forward` 通过 `new Node<K, V>*[level + 1]` 在 `skiplist.h:66` 分配

释放路径：

1. `~Node()` 在 `skiplist.h:76-82`
2. `forward` 通过 `delete[] forward` 释放

### 2.3 `SkipList<K, V>` 拥有的资源

定义位置：

1. `skiplist.h:100-145`

成员资源：

1. `_header`
2. `_file_writer`
3. `_file_reader`
4. `_max_level`
5. `_skip_list_level`
6. `_element_count`

所有权关系：

1. `_header` 是整张跳表节点链的根入口，`SkipList` 对它拥有所有权。
2. 所有业务节点都通过 `_header->forward[0]` 可达，本质上也由 `SkipList` 间接拥有。
3. `_file_writer` 和 `_file_reader` 是文件流成员，生命周期与 `SkipList` 对象一致。
4. 标量成员不需要显式释放。

## 3. 生命周期分阶段说明

### 3.1 程序加载阶段

在包含 `skiplist.h` 的翻译单元初始化时，会创建：

1. 全局互斥锁 `mtx`
2. 全局字符串 `delimiter`

这意味着资源生命周期早于任何 `SkipList` 实例，也晚于任何 `SkipList` 实例结束。

### 3.2 `SkipList` 构造阶段

构造函数位置：

1. `skiplist.h:510-526`

执行流程：

1. 初始化 `_max_level`
2. 初始化 `_skip_list_level`
3. 初始化 `_element_count`
4. 构造一个头节点 `_header`

头节点创建细节：

1. `SkipList` 构造函数调用 `new Node<K, V>(k, v, _max_level)`。
2. 头节点自己的 `forward` 数组也会在 `Node` 构造函数中单独分配。
3. 此时跳表对象至少拥有一块长期堆资源：头节点，以及头节点的 `forward` 数组。

### 3.3 插入阶段

关键路径：

1. `skiplist.h:186-280`

涉及资源：

1. 全局锁 `mtx`
2. 栈上临时数组 `update[_max_level + 1]`
3. 新节点对象 `inserted_node`

执行顺序：

1. `mtx.lock()` 获取全局锁。
2. 在栈上创建 `update` 数组，记录每层前驱。
3. 如果 key 不存在，调用 `create_node`。
4. `create_node` 内部通过 `new` 申请节点对象。
5. 节点构造时再次通过 `new[]` 申请自己的 `forward` 数组。
6. 新节点被链接进跳表。
7. `_element_count` 增加。
8. `mtx.unlock()` 释放锁。

所有权转移：

1. `create_node` 返回的裸指针先由调用方临时持有。
2. 一旦链接进链表，这个节点就转为由整个 `SkipList` 管理。

### 3.4 删除阶段

关键路径：

1. `skiplist.h:395-455`

涉及资源：

1. 全局锁 `mtx`
2. 栈上临时数组 `update[_max_level + 1]`
3. 待删除节点 `current`

执行顺序：

1. `mtx.lock()` 获取全局锁。
2. 在栈上构造 `update` 数组。
3. 查找目标节点及各层前驱。
4. 把各层前驱的 `forward` 指针改到目标节点后继。
5. 调用 `delete current`。
6. 节点析构函数释放它自己的 `forward` 数组。
7. `_element_count` 减一。
8. `mtx.unlock()` 释放锁。

释放语义：

1. 删除操作只释放目标节点自己。
2. 其他节点的所有权不变。

### 3.5 遍历与查询阶段

相关函数：

1. `display_list` 在 `skiplist.h:282-296`
2. `search_element` 在 `skiplist.h:476-508`

特点：

1. 这两个函数不分配节点内存。
2. 它们只借用现有节点指针进行遍历。
3. 如果没有并发修改，它们不改变对象所有权。

### 3.6 落盘阶段

关键路径：

1. `skiplist.h:298-321`

涉及资源：

1. 成员文件流 `_file_writer`
2. 遍历指针 `node`

执行顺序：

1. `_file_writer.open(STORE_FILE)` 打开文件。
2. 顺着第 0 层遍历全部节点。
3. 把 `key:value` 文本写入文件。
4. `flush()`
5. `close()`

生命周期特点：

1. 文件流对象本身在 `SkipList` 内长期存在。
2. 底层文件句柄只在 `open` 到 `close` 之间持有。

### 3.7 加载阶段

关键路径：

1. `skiplist.h:323-352`

涉及资源：

1. 成员文件流 `_file_reader`
2. 临时堆对象 `key`
3. 临时堆对象 `value`
4. 每一行解析后可能新插入的节点

执行顺序：

1. `_file_reader.open(STORE_FILE)` 打开文件。
2. 通过 `new std::string()` 申请 `key`。
3. 通过 `new std::string()` 申请 `value`。
4. 循环读取文件每一行。
5. 解析成功后调用 `insert_element`，可能进一步申请新节点。
6. 循环结束后 `delete key`。
7. 循环结束后 `delete value`。
8. `_file_reader.close()` 关闭文件。

生命周期特点：

1. `key` 和 `value` 是方法级临时堆对象，不属于 `SkipList` 长期状态。
2. 这里本可以使用栈对象，但当前实现选择了手工 `new/delete`。

### 3.8 析构阶段

关键路径：

1. `skiplist.h:528-566`

执行顺序：

1. `~SkipList()` 从 `_header->forward[0]` 开始释放整条第 0 层链。
2. 递归函数 `clear` 先递归到尾，再从尾到头逐个 `delete` 普通节点。
3. 每个节点析构时释放自己的 `forward` 数组。
4. 普通节点删完后，再 `delete _header`。
5. 头节点析构时释放头节点自己的 `forward` 数组。
6. `_file_writer` 和 `_file_reader` 作为成员对象，随后由其自身析构函数收尾。

所有权终止顺序：

1. 普通业务节点
2. 头节点
3. 文件流成员

## 4. 当前所有权模型总结

可以把当前实现理解成一套“手工所有权模型”：

1. `SkipList` 拥有 `_header`。
2. `_header` 通过第 0 层链表把全部业务节点串起来。
3. 每个 `Node` 拥有自己的 `forward` 数组。
4. 插入时资源从“临时裸指针”变成“链表托管”。
5. 删除时先从链表摘链，再释放节点。
6. 析构时通过第 0 层递归遍历释放所有节点。

## 5. 临时资源与长期资源的区别

长期资源：

1. 全局 `mtx`
2. 全局 `delimiter`
3. `SkipList` 对象
4. `_header`
5. 所有已插入节点
6. `_file_writer`
7. `_file_reader`

短期资源：

1. `insert_element` 中的 `update` 栈数组
2. `delete_element` 中的 `update` 栈数组
3. 遍历用的 `current`、`node`
4. `load_file` 中的 `key`、`value`
5. `dump_file` 和 `load_file` 打开期间对应的底层文件句柄

## 6. 阅读这份代码时要记住的三个事实

1. 资源管理核心依赖的是裸指针和手工 `delete`，不是 RAII 风格的智能指针所有权。
2. 节点释放逻辑只认第 0 层链，因此“第 0 层完整可达”是析构正确性的关键前提。
3. 文件流成员本身是 RAII 对象，但 `load_file` 里的临时字符串和节点内存仍然走手工管理路径。
