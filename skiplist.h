/* ************************************************************************
> File Name:     skiplist.h
> Author:        程序员Carl
> 微信公众号:    代码随想录
> Created Time:  Sun Dec  2 19:04:26 2018
> Description:   
 ************************************************************************/
#pragma once

#include <algorithm>
#include <iostream> 
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <mutex>
#include <fstream>
#include <optional>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

// #define STORE_FILE "store/dumpFile" // 定义持久化文件路径

// std::mutex mtx; 头文件全局锁，粒度较大。最终所有 skipList 对象共享同一个锁，可能会导致性能瓶颈。
// std::string delimiter = ":"; // 定义键值对分隔符

namespace skiplist {
// Class template to implement node
/**
 * @brief 跳表中节点模板类。
 * 
 * @tparam K：键类型。
 * @tparam V：值类型。
 */
template<typename K, typename V> 
class Node {

public:
    
    Node() {} // 默认构造函数

    Node(K k, V v, int); // 构造函数，参数为键、值和节点层数

    ~Node(); // 析构函数

    K get_key() const;

    V get_value() const;

    void set_value(V);
    
    // Linear array to hold pointers to next node of different level
    Node<K, V> **forward;
    // 公有成员：forward数组，保存在不同层级上当前节点的下一个节点指针

    int node_level; // 当前节点所在的层数

private:
    K key;
    V value;
};

template<typename K, typename V> 
Node<K, V>::Node(const K k, const V v, int level) {
    this->key = k;
    this->value = v;
    this->node_level = level;

    // level + 1, because array index is from 0 - level
    this->forward = new Node<K, V>*[level + 1];
    // forward 维护一个长度为 level + 1 的指针数组，forward[i] 指向当前节点在第 i 层的下一个节点
    // 当前节点的 level 越高，forward 中的指针数量越多。
    // this->forward.resize(level + 1, nullptr); // 初始化 forward 数组，所有指针初始为 nullptr
    
	// Fill forward array with 0(NULL)
    // memset(this->forward, NULL, sizeof(Node<K, V>*)*(level+1));
    std::fill_n(this->forward, level + 1, nullptr);
    // 默认 forward 中的各层次的 next 指针都指向 nullptr，表示当前节点在各层次上没有下一个节点。
};

template <typename K, typename V> Node<K, V>::~Node() {
    /**
     * @brief 节点析构函数
     * 删除当前节点的 forward 数组，释放内存。
     */
    delete []forward;
};

template<typename K, typename V> 
K Node<K, V>::get_key() const {
    return key;
};

template<typename K, typename V> 
V Node<K, V>::get_value() const {
    return value;
};
template<typename K, typename V> 
void Node<K, V>::set_value(V value) {
    this->value = value;
};

struct SkipListOptions {
    int max_level; // 跳表的最大层数
    std::string store_file; // 持久化文件路径
    std::string wal_path;   // WAL 日志文件路径
    bool enable_wal;        // 是否启用 WAL 日志
    bool sync_wal;          // 是否在每次写入 WAL 日志后立即同步到磁盘
    bool enable_debug_output; // 是否启用调试输出
    
    // 默认构造函数
    SkipListOptions()
        : max_level(16), store_file("store/dumpFile"), wal_path("store/walFile"), enable_wal(false),
          sync_wal(false), enable_debug_output(false) {}
    
    // 便捷构造函数，仅指定 max_level
    explicit SkipListOptions(int ml)
        : max_level(ml), store_file("store/dumpFile"), wal_path("store/walFile"), enable_wal(false),
          sync_wal(false), enable_debug_output(false) {}
    
    // 完整构造函数
    SkipListOptions(int max_level, const std::string& store_file, const std::string& wal_path, bool enable_wal,
                    bool sync_wal, bool enable_debug_output)
        : max_level(max_level), store_file(store_file), wal_path(wal_path), enable_wal(enable_wal),
          sync_wal(sync_wal), enable_debug_output(enable_debug_output) {}
};

// 跳表持久化 traits，默认不支持持久化
template <typename K, typename V>
struct persistence_supported : std::false_type {}; // 默认不支持持久化

// 对于 int 和 std::string 类型的键值对
// 特化 persistence_supported 结构体，设定为 true_type，表示支持持久化。
template <>
struct persistence_supported<int, std::string> : std::true_type {}; // int 和 std::string 类型支持持久化

// 定义一个 constexpr 变量，方便在代码中检查是否支持持久化。
template <typename K, typename V>
inline constexpr bool is_persistence_supported_v = persistence_supported<K, V>::value;

enum class WriteResult : uint8_t {
    inserted, // 表示新元素被成功插入
    updated,  // 表示已有元素的值被更新
};

inline const char* to_string(WriteResult result) {
    switch (result) {
        case WriteResult::inserted:
            return "inserted";
        case WriteResult::updated:
            return "updated";
        default:
            return "unknown";
    }
}

inline std::ostream& operator<<(std::ostream& os, WriteResult result) {
    os << to_string(result);
    return os;
}


// Class template for Skip list
// 跳表模板类
template <typename K, typename V> 
class SkipList {

public:
    explicit SkipList(const SkipListOptions& options);
    SkipList(int);
    ~SkipList();
    int get_random_level(); // 生成随机层数
    Node<K, V>* create_node(K, V, int); // 创建新节点
    int insert_element(K, V); // 插入元素，有锁
    void display_list(std::ostream& os = std::cout) const; // 显示跳表，无锁
    bool search_element(K); // 搜索元素，无锁
    void delete_element(K); // 删除元素，有锁
    void dump_file(); // 将跳表数据持久化到文件
    void load_file(); // 从文件加载数据到跳表
    //递归删除节点
    // void clear(Node<K,V>*); // 递归删除跳表中的节点
    int size(); // 获取跳表中元素的数量

    // 以下为新主接口，旧 API 只做兼容，最终会废弃。
    size_t size() const; // 新主接口，获取跳表中元素的数量，线程安全
    std::optional<V> get(const K &key) const; // 新主接口，获取指定键的值，线程安全
    bool contains(const K &key) const;        // 新主接口，检查跳表中是否包含指定键，线程安全
    bool erase(const K &key);                 // 新主接口，删除指定键的元素，线程安全
    std::vector<std::pair<K, V>> scan(const K &start_key,
                                      const K &end_key) const; // 新主接口，范围查询，线程安全
    bool checkpoint(); // 新主接口，执行一次全量持久化，将当前跳表状态保存到文件中，线程安全
    bool recover();    // 新主接口，从持久化文件中恢复跳表状态，线程安全
    WriteResult put(const K &key, const V &value); // 新主接口，插入或更新元素，线程安全，返回写入结果（inserted 或 updated）

private:
    std::optional<V> get_unlocked(const K &key) const;
    WriteResult put_unlocked(const K &key, const V &value, bool update_existing);
    bool erase_unlocked(const K &key);
    void dump_file_unlocked();
    void load_file_unlocked();

    bool append_wal_line_unlocked(const std::string &line);
    bool append_put_wal_unlocked(const K& key, const V& value);
    bool append_delete_wal_unlocked(const K &key);

    bool replay_wal_unlocked();

private:
    // 定义快照文件的头部标识，用于验证文件格式和版本，确保在加载数据时能够正确识别和解析文件内容。
    static constexpr const char *kSnapshotHeader = "SKIPLIST_SNAPSHOT_V1";
    // 定义字段分隔符，使用制表符 '\t' 作为键值对之间的分隔符，避免与键值中的常见字符冲突，提高数据解析的可靠性。
    static constexpr char kFieldSep = '\t';
    // 定义 WAL 日志中表示插入或更新操作的标识符，使用 'P' 来区分不同类型的操作，便于在日志中记录和解析写入操作。
    static constexpr char kWalPutOp = 'P';
    // 定义 WAL 日志中表示删除操作的标识符，使用 'D' 来区分不同类型的操作，便于在日志中记录和解析删除操作。
    static constexpr char kWalDeleteOp = 'D';

    // 定义转义函数，用于处理键值对中可能包含的分隔符，避免在解析时出现问题
    static std::string escape_value(const std::string &value);
    // 定义反转义函数，用于将转义后的字符串还原为原始值，确保在加载数据时能够正确解析出键值对。
    // 通过返回 bool 判断是否转义失败。若失败，则记录 recover() == false，并在日志中输出错误信息，便于后续排查问题。
    static bool unescape_value(const std::string &escaped, std::string *unescaped);
    // 解析字符串中的整数键，返回解析结果
    static bool parse_int_key(const std::string &str, int *key);
    // 按行编码 snapshot 文件内容，返回编码后的字符串
    static std::string encode_snapshot_line(int key, const std::string& line);
    // 按行解码 snapshot 文件内容，返回解析是否成功，并通过参数返回解析出的键值对
    static bool decode_snapshot_line(const std::string &line, int *key, std::string *value);

    // 编码 WAL 日志 put 记录，返回编码后的字符串
    static std::string encode_wal_put_record(int key, const std::string &value);
    // 编码 WAL 日志 delete 记录，返回编码后的字符串
    static std::string encode_wal_delete_record(int key);
    // 解码 WAL 日志记录，返回解析是否成功，并通过参数返回解析出的操作类型、键和值
    static bool
    decode_wal_record(const std::string &line, char *op_type, int *key, std::string *value);
    
    // 删除所有节点，替代原始的 clear 函数，避免递归删除导致的栈溢出问题。
    void clear_all_nodes();
    // 删除数据节点，保留头节点，适用于 load_file_unlocked 之前清空旧数据的场景。
    void clear_data_nodes_unlocked();

    bool _enable_wal; // 是否启用 WAL 日志
    bool _sync_wal;   // 是否在每次写入 WAL 日志后立即同步到磁盘
    
    // Maximum level of the skip list
    // 跳表的最大层数，决定了跳表的高度和性能。
    // 较大的 max_level 可以提高搜索效率，但也会增加内存使用。
    int _max_level;

    // current level of skip list
    // 当前跳表的实际层数，随着元素的插入和删除而动态调整。
    int _skip_list_level;

    // pointer to header node
    // 指向当前跳表的头节点
    Node<K, V> *_header;

    // file operator
    // 文件操作对象，用于将跳表数据持久化到文件和从文件加载数据。
    // 设定为成员变量，方便在跳表的生命周期内进行文件操作。
    std::ofstream _file_writer;
    std::ifstream _file_reader;

    // 持久化文件路径
    std::string _store_file;

    // WAL 日志文件路径
    std::string _wal_file;

    // skiplist current element count
    // 当前跳表中的元素数量，用于统计跳表的大小。
    int _element_count;

    // 设定为成员的互斥锁，保护跳表的修改操作，确保线程安全。
    mutable std::shared_mutex mtx_; // 全表级读写锁，允许多个线程同时读取，但在写入时独占锁，提升并发性能。
};

// create new node
template <typename K, typename V>
Node<K, V> *SkipList<K, V>::create_node(const K k, const V v, int level) {
    /**
     * @brief 创建新节点
     * @param k：节点的键。
     * @param v：节点的值。
     * @param level：节点的层数。
     * @return Node<K, V>*：指向新创建节点的指针。
     * 节点创建时，level 已经确定。
     */
    Node<K, V> *n = new Node<K, V>(k, v, level);
    return n;
}

// Insert given key and value in skip list 
// return 1 means element exists  
// return 0 means insert successfully
/* 
                           +------------+
                           |  insert 50 |
                           +------------+
level 4     +-->1+                                                      100
                 |
                 |                      insert +----+
level 3         1+-------->10+---------------> | 50 |          70       100
                                               |    |
                                               |    |
level 2         1          10         30       | 50 |          70       100
                                               |    |
                                               |    |
level 1         1    4     10         30       | 50 |          70       100
                                               |    |
                                               |    |
level 0         1    4   9 10         30   40  | 50 |  60      70       100
                                               +----+

*/
template<typename K, typename V>
int SkipList<K, V>::insert_element(const K key, const V value) {
    /**
     * @brief 跳表插入节点
     * @details 先找到插入位置，再生成随机层数，最后调整指针完成插入。
     * @param key：要插入节点的键。
     * @param value：要插入节点的值。
     * @return int：返回 1 表示元素已存在，返回 0 表示插入成功。
     */

    std::unique_lock<std::shared_mutex> lock(mtx_);
    // 先查重
    if (get_unlocked(key).has_value()) {
        return 1; // 元素已存在
    }
    // 如果启用 WAL 日志，则在插入之前记录插入操作到 WAL 日志中，以便在发生崩溃时能够通过 WAL 日志进行恢复。
    if (_enable_wal) {
        if (!append_put_wal_unlocked(key, value)) {
            throw std::runtime_error("failed to append WAL record for key: " + std::to_string(key));
        }
    }
    // 最后调用 put_unlocked 执行插入操作，传入 update_existing = false，表示这是一个插入操作，不会更新已有元素。
    put_unlocked(key, value, false);
    return 0; // 插入成功
}

// Display skip list
template <typename K, typename V> void SkipList<K, V>::display_list(std::ostream &os) const {
    /**
     * @brief 显示跳表内容
     * @details 从最高层开始，逐层打印跳表中的节点信息，每层显示节点的键值对。
     * @param os：输出流对象，默认为 std::cout，可以指定其他输出流，例如文件流。
     * @note 读路径，需要加共享锁，确保在读取跳表内容时不会被修改，保持数据一致性。
     */
    std::shared_lock<std::shared_mutex> lock(mtx_); // 读路径加共享锁，允许多个线程同时读取，但在写入时独占锁，提升并发性能。
    os << "\n*****Skip List*****"<<"\n"; 
    for (int i = 0; i <= _skip_list_level; i++) {
        Node<K, V> *node = this->_header->forward[i]; 
        os << "Level " << i << ": ";
        while (node != NULL) {
            os << node->get_key() << ":" << node->get_value() << ";";
            node = node->forward[i];
        }
        os << '\n';
    }
}

// Dump data in memory to file
template <typename K, typename V> void SkipList<K, V>::dump_file() {
    /**
     * @brief 持久化跳表内容到文件，快照式持久化
     * @details 第一行固定写入 snapshot header，后续每行使用 "<key>\t<escaped_value>" 格式。
     * @note 当前仍是全量 snapshot，不包含 WAL 回放逻辑。
     */
    // 检查约束条件，确保当前 SkipList 的键值类型支持持久化。
    static_assert(is_persistence_supported_v<K, V>, "Current key-value types do not support persistence in dump_file.");
    
    std::shared_lock<std::shared_mutex> lock(mtx_);
    dump_file_unlocked();
}

// Load data from disk
template <typename K, typename V> void SkipList<K, V>::load_file() {
    /**
     * @brief 从文件加载数据到跳表
     * @details 从指定的文件中读取数据，解析每行的键值对，并将其插入到跳表中。
     */
    // 检查约束条件，确保当前 SkipList 的键值类型支持持久化。
    static_assert(is_persistence_supported_v<K, V>,
                  "Current key-value types do not support persistence in load_file.");
    
    std::unique_lock<std::shared_mutex> lock(mtx_);
    load_file_unlocked();
}

// Get current SkipList size
template<typename K, typename V> 
int SkipList<K, V>::size() { 
    return static_cast<int>(static_cast<const SkipList<K, V>&>(*this).size());
}

// Delete element from skip list
template <typename K, typename V>
void SkipList<K, V>::delete_element(K key) {
    /**
     * @brief 删除跳表中的元素
     * @param key：要删除的键
     * @details 先找到要删除的节点，记录需要调整的节点位置，然后调整 forward 指针完成删除，最后更新跳表的实际层数和元素数量。
     */
    erase(key);
}

// Search for element in skip list 
/*
                           +------------+
                           |  select 60 |
                           +------------+
level 4     +-->1+                                                      100
                 |
                 |
level 3         1+-------->10+------------------>50+           70       100
                                                   |
                                                   |
level 2         1          10         30         50|           70       100
                                                   |
                                                   |
level 1         1    4     10         30         50|           70       100
                                                   |
                                                   |
level 0         1    4   9 10         30   40    50+-->60      70       100
*/
template <typename K, typename V>
bool SkipList<K, V>::search_element(K key) {
    /**
     * @brief 在跳表中搜索元素
     * @param key 要搜索的键
     * @return 如果找到返回 true，否则返回 false
     * @note 读取过程中没有加锁，如果并发修改跳表，可能会导致读取到不一致的数据，后续改进可以使用读写锁来实现更细粒度的锁定，允许多个线程同时读取，但在写入时独占锁。
     */
    return contains(key);
}

template <typename K, typename V>
SkipList<K, V>::SkipList(const SkipListOptions& options) {
    /**
     * @brief 跳表构造函数
     * @param options 跳表配置选项，包括最大层数、持久化文件路径、WAL 日志配置等。
     * @details 根据传入的配置选项初始化跳表的成员变量，并创建头节点。
     */
    this->_max_level = options.max_level;
    this->_skip_list_level = 0;
    this->_element_count = 0;
    this->_store_file = options.store_file;
    this->_wal_file = options.wal_path;
    this->_enable_wal = options.enable_wal;
    this->_sync_wal = options.sync_wal;
    assert(!this->_store_file.empty());

    // create header node and initialize key and value to null
    K k;
    V v;
    this->_header = new Node<K, V>(k, v, _max_level);
};


// construct skip list
// 构造函数，委托给另一个构造函数，使用指定的 max_level 初始化跳表。
template <typename K, typename V>
SkipList<K, V>::SkipList(int max_level) : SkipList(SkipListOptions(max_level)) {}

template <typename K, typename V> SkipList<K, V>::~SkipList() {
    /**
     * @brief 析构函数
     * @details 释放跳表占用的资源，包括文件操作对象和跳表节点。
     * @note 这里的文件流显示关闭是冗余的，不符合 RAII 原则。因为当跳表对象被销毁时，成员的析构函数会自动调用，文件操作对象会自动关闭文件，所以不需要显式地调用 close() 方法。
     * @note 删除过程也不是异常安全的。如果在删除节点过程中发生了异常，可能会导致内存泄漏或者部分节点未被正确删除。
     * @note 后续改进可以使用智能指针来管理节点的生命周期，确保在发生异常时能够正确释放资源。
     */

    // 析构函数，释放跳表占用的资源，包括文件操作对象和跳表节点。
    // 冗余操作，当跳表对象被销毁时，成员的析构函数会自动调用，文件操作对象会自动关闭文件，所以不需要显式地调用 close() 方法。
    // if (_file_writer.is_open()) {
    //     _file_writer.close();
    // }
    // if (_file_reader.is_open()) {
    //     _file_reader.close();
    // }

    // 递归删除跳表链条，从头节点的第 0 层开始，依次删除每个节点，最后删除头节点。
    // 异常不安全，如果在删除节点过程中发生了异常，可能会导致内存泄漏或者部分节点未被正确删除。
    // if (_header->forward[0] != nullptr) {
    //     clear(_header->forward[0]);
    // }
    // // 最后删除头节点
    // delete(_header);
    clear_all_nodes();
}

// template <typename K, typename V>
// void SkipList<K, V>::clear(Node<K, V> *cur) {
//     /**
//      * @brief 递归删除跳表中的节点
//      * @param cur：当前要删除的节点
//      * @note 递归删除在数据量较大时，可能存在栈溢出的风险，后续改进可以使用迭代的方式来删除节点，避免递归带来的问题。
//      */
//     if (cur->forward[0] != nullptr) {
//         // 递归进入下一层级，删除当前节点的下一个节点，直到到达第 0 层的末尾。
//         clear(cur->forward[0]);
//     }
//     delete(cur);
// }

template <typename K, typename V>
void SkipList<K, V>::clear_all_nodes() {
    /**
     * @brief 迭代删除跳表中的所有节点
     * @details 从头节点的第 0 层开始，依次删除每个节点，最后删除头节点。
     * @note 这种方式避免了递归带来的栈溢出风险，适用于数据量较大的情况。
     */
    // 从第一个节点开始
    Node<K, V>* current = _header->forward[0];
    // 遍历第 0 层的所有节点，依次删除
    while (current) {
        Node<K, V> *next = current->forward[0];
        delete current;
        current = next;
    }
    // 最后删除头节点
    delete _header;
}

template <typename K, typename V>
void SkipList<K, V>::clear_data_nodes_unlocked() {
    Node<K, V>* current = _header->forward[0];
    while (current) {
        Node<K, V>* next = current->forward[0];
        delete current;
        current = next;
    }
    std::fill_n(_header->forward, _max_level + 1, nullptr);
    _skip_list_level = 0;
    _element_count = 0;
}

template<typename K, typename V>
int SkipList<K, V>::get_random_level() {

    /**
     * @brief 随机生成节点的层数
     * @details 通过抛硬币的方式随机生成层数，平均层数为 2。
     * k 的值遵循几何分布，P(k) = 0.5^k，k 从 1 开始。
     * 每次抛硬币，如果是正面（rand() % 2 == 0），则 k 增加 1；如果是反面，则停止。
     */
    int k = 1;
    while (rand() % 2) {
        k++;
    }
    // 若 k 超过了跳表的最大层数 _max_level，则将 k 限制为 _max_level
    // 确保新节点的层数不会超过跳表的最大层数。
    k = (k < _max_level) ? k : _max_level;
    return k;
};
// vim: et tw=100 ts=4 sw=4 cc=120

template<typename K, typename V>
std::string SkipList<K, V>::escape_value(const std::string &value) {
    /**
     * @brief 转义所有分隔符和特殊字符
     * @param value：待转义的字符串
     * @return 转义后的字符串
     * @details 将所有转义字符（如反斜杠、制表符、换行符等）进行转义，确保在存储和解析过程中不会被误解为分隔符或其他特殊字符。
     */
    std::string res;
    res.reserve(value.size() * 2); // 预分配足够的空间，避免频繁 realloc
    for (char c : value) {
        switch (c) {
            case '\\':
                res += "\\\\"; // 转义反斜杠
                break;
            case '\t':
                res += "\\t"; // 转义制表符
                break;
            case '\n':
                res += "\\n"; // 转义换行符
                break;
            default:
                res.push_back(c); // 其他字符直接添加
                break;
        }
    }
    return res;
}

template<typename K, typename V>
bool SkipList<K, V>::unescape_value(const std::string &value, std::string *unescaped) {
    /**
     * @brief 反转义字符串中的分隔符
     * @param value：待反转义的字符串
     * @param unescaped：存储反转义后字符串的指针
     * @return bool：如果反转义成功返回 true，否则返回 false
     * @details 将字符串中的转义序列（如 "\\t"、"\\n"、"\\\\" 等）还原为原始字符，确保在加载数据时能够正确解析出键值对。
     */
    unescaped->clear();
    unescaped->reserve(value.size()); // 预分配足够的空间，避免频繁 realloc
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\') {
            if (i + 1 >= value.size()) {
                return false; // 转义字符后面没有字符，反转义失败
            }
            switch (value[i + 1]) {
                case 't':
                    unescaped->push_back('\t');
                    break;
                case 'n':
                    unescaped->push_back('\n');
                    break;
                case '\\':
                    unescaped->push_back('\\');
                    break;
                default:
                    return false; // 未知的转义序列，反转义失败
            }
            ++i; // 跳过转义字符
        } else {
            unescaped->push_back(value[i]);
        }
    }
    return true;
}

template <typename K, typename V>
size_t SkipList<K, V>::size() const {
    /**
     * @brief 获取跳表中元素的数量
     * @return 跳表中元素的数量
     * @details 通过访问成员变量 _element_count 获取当前跳表中元素的数量。
     * @note 由于 _element_count 是在插入和删除操作中更新的，因此在读取时需要加锁以确保线程安全。
     */
    // 读取方法，用共享锁
    std::shared_lock lock(mtx_);
    return _element_count;
}

template <typename K, typename V>
std::optional<V> SkipList<K, V>::get_unlocked(const K &key) const {
    Node<K, V> *current = _header;
    for (int i = _skip_list_level; i >= 0; --i) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
    }
    current = current->forward[0];
    if (current && current->get_key() == key) {
        return current->get_value();
    }
    return std::nullopt;
}

template <typename K, typename V> std::optional<V> SkipList<K, V>::get(const K &key) const {
    /**
     * @brief 获取指定键的值
     * @param key：要获取值的键
     * @return std::optional<V>：如果找到返回包含值的 std::optional，否则返回 std::nullopt
     * @details 通过在跳表中搜索指定键，找到对应的节点并返回其值。如果找不到，则返回 std::nullopt。
     * @note 读取方法，用共享锁
     */
    std::shared_lock lock(mtx_);
    return get_unlocked(key);
}

template <typename K, typename V> bool SkipList<K, V>::contains(const K &key) const {
    /**
     * @brief 检查跳表中是否包含指定键
     * @param key：要检查的键
     * @return bool：如果包含返回 true，否则返回 false
     * @details 通过在跳表中搜索指定键，判断是否存在对应的节点来确定是否包含该键。
     * @note 读取方法，用共享锁
     */
    std::shared_lock lock(mtx_);
    return get_unlocked(key).has_value();
}

template <typename K, typename V> bool SkipList<K, V>::erase_unlocked(const K &key) {
    /**
     * @brief 删除指定键的元素
     * @param key：要删除的键
     * @return bool：如果删除成功返回 true，否则返回 false
     * @details 通过在跳表中搜索指定键，找到对应的节点并删除它。如果找不到，则返回 false。
     * @note 写入方法，用独占锁
     */
    Node<K, V> *current = _header;
    Node<K, V> *update[_max_level + 1];
    std::fill_n(update, _max_level + 1, nullptr);
    for (int i = _skip_list_level; i >= 0; --i) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];
    if (current && current->get_key() == key) {
        for (int i = 0; i <= _skip_list_level; ++i) {
            if (update[i]->forward[i] != current) {
                break;
            }
            update[i]->forward[i] = current->forward[i];
        }
        while (_skip_list_level > 0 && _header->forward[_skip_list_level] == nullptr) {
            --_skip_list_level;
        }
        delete current;
        --_element_count;
        return true;
    }
    return false;
}

template <typename K, typename V> bool SkipList<K, V>::erase(const K &key) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    // 检查是否存在 key，如果不存在则直接返回 false，避免不必要的删除操作和锁竞争。
    if (!get_unlocked(key).has_value()) {
        return false;
    }
    // 如果启用 WAL 日志，则在删除之前记录删除操作到 WAL 日志中，以便在发生崩溃时能够通过 WAL 日志进行恢复。
    if (_enable_wal) {
        if (!append_delete_wal_unlocked(key)) {
            return false; // 如果记录 WAL 日志失败，则返回 false，避免执行删除操作导致数据不一致。
        }
    }
    return erase_unlocked(key);
}

template <typename K, typename V>
std::vector<std::pair<K, V>> SkipList<K, V>::scan(const K &start_key, const K &end_key) const {
    /**
     * @brief 范围查询
     * @param start_key：范围的起始键，包含在范围内
     * @param end_key：范围的结束键，不包含在范围内
     * @return std::vector<std::pair<K, V>>：包含左闭右开范围内所有键值对的向量
     * @details 从跳表中查找所有键在 [start_key, end_key) 范围内的元素，并返回它们的键值对。
     * @note 读取方法，用共享锁
     */
    std::shared_lock lock(mtx_);
    std::vector<std::pair<K, V>> result;
    Node<K, V> *current = _header;
    for (int i = _skip_list_level; i >= 0; --i) {
        while (current->forward[i] && current->forward[i]->get_key() < start_key) {
            current = current->forward[i];
        }
    }
    current = current->forward[0];
    // 到达第 0 层，current->forward[0] 是第 0 层上第一个键大于或等于 start_key 的节点。
    while (current && current->get_key() < end_key) {
        if (current->get_key() >= start_key) {
            result.emplace_back(current->get_key(), current->get_value());
        }
        current = current->forward[0];
    }
    return result;
}

template <typename K, typename V>
bool SkipList<K, V>::checkpoint() {
    /**
     * @brief 创建跳表的检查点
     * @return bool：如果检查点创建成功返回 true，否则返回 false
     * @details snapshot 写完并同步后，清空旧的 WAL 文件，新的恢复起点变为“snapshot + 空 WAL”。
     * @note checkpoint 完成后，新的恢复起点为 snapshot + 空 WAL。
     */
    // 检查约束条件，确保当前 SkipList 的键值类型支持持久化。
    static_assert(is_persistence_supported_v<K, V>, "Current key-value types do not support persistence in checkpoint.");
    // 将当前跳表的状态持久化到文件中，创建检查点。需要写锁，即独占锁，确保在创建检查点时跳表不会被修改，保持数据一致性。
    std::unique_lock<std::shared_mutex> lock(mtx_);
    // 创建检查点，通过调用 dump_file 方法将跳表中的数据持久化到文件中。
    // 如果 dump_file 成功完成，则返回 true；如果发生异常或者失败，则返回 false。
    try {
        dump_file_unlocked();
        // 确保 snapshot 的数据已经完全写入磁盘，避免在后续清空 WAL 文件后，发生崩溃导致恢复失败。
        const int snapshot_fd = ::open(_store_file.c_str(), O_WRONLY);
        if (snapshot_fd == -1) {
            throw std::runtime_error("failed to reopen snapshot file for fsync: " + _store_file);
        }
        if (::fsync(snapshot_fd) == -1) {
            ::close(snapshot_fd);
            throw std::runtime_error("failed to fsync snapshot file: " + _store_file);
        }
        if (::close(snapshot_fd) == -1) {
            throw std::runtime_error("failed to close snapshot file after fsync: " + _store_file);
        }
        // 清空旧的 WAL 文件，确保新的恢复起点为 snapshot + 空 WAL。
        // 通过以 O_TRUNC 模式打开 WAL 文件来清空其内容。
        const int wal_fd = ::open(_wal_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (wal_fd == -1) {
            throw std::runtime_error("failed to truncate WAL file: " + _wal_file);
        }
        // 确保 WAL 文件的内容已经完全清空并写入磁盘，避免在后续发生崩溃导致恢复失败。
        if (::fsync(wal_fd) == -1) {
            ::close(wal_fd);
            throw std::runtime_error("failed to fsync truncated WAL file: " + _wal_file);
        }
        if (::close(wal_fd) == -1) {
            throw std::runtime_error("failed to close truncated WAL file: " + _wal_file);
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Checkpoint failed: " << e.what() << '\n';
        return false;
    }
}

template <typename K, typename V>
bool SkipList<K, V>::recover() {
    /**
     * @brief 从检查点恢复跳表
     * @return bool：如果恢复成功返回 true，否则返回 false
     * @details 通过调用 load_file 方法从持久化文件中加载数据，恢复跳表的状态。
     * @note 先清空跳表节点，保留头节点。若 snapshot 存在则先加载 snapshot，再在 WAL 存在时顺序回放 WAL。
     */
    // 检查约束条件，确保当前 SkipList 的键值类型支持持久化。
    static_assert(is_persistence_supported_v<K, V>, "Current key-value types do not support persistence in recover.");
    std::unique_lock<std::shared_mutex> lock(mtx_);
    try {
        clear_data_nodes_unlocked();

        std::ifstream snapshot_file(_store_file);
        if (snapshot_file.good()) {
            load_file_unlocked();
        }
        snapshot_file.close();

        std::ifstream wal_file(_wal_file);
        // 如果 WAL 文件存在且可读，则回放 WAL 日志以恢复数据。若回放失败，则返回 false，表示恢复失败。
        if (wal_file.good() && !replay_wal_unlocked()) {
            wal_file.close();
            return false;
        }
        wal_file.close();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Recovery failed: " << e.what() << '\n';
        return false;
    }
}

template <typename K, typename V>
WriteResult SkipList<K, V>::put_unlocked(const K &key, const V &value, bool update_existing) {
    /**
     * @brief 插入或更新键值对
     * @param key：要插入或更新的键
     * @param value：要插入或更新的值
     * @return WriteResult：表示写入结果的枚举值，可能是 Inserted、Updated 或 Failed
     * @details 如果键不存在，则插入新的键值对并返回 Inserted；如果键已存在，则更新其值并返回 Updated；如果发生错误，则返回 Failed。
     */
    Node<K, V> *current = _header;
    Node<K, V> *update[_max_level + 1];
    std::fill_n(update, _max_level + 1, nullptr);
    for (int i = _skip_list_level; i >= 0; --i) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];
    if (current && current->get_key() == key) {
        if (update_existing) {
            current->set_value(value);
        }
        return WriteResult::updated;
    }

    int random_level = get_random_level();
    if (random_level > _skip_list_level) {
        for (int i = _skip_list_level + 1; i <= random_level; ++i) {
            update[i] = _header;
        }
        _skip_list_level = random_level;
    }
    Node<K, V>* new_node = create_node(key, value, random_level);
    for (int i = 0; i <= random_level; ++i) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }
    ++_element_count;
    return WriteResult::inserted;
}

template <typename K, typename V> WriteResult SkipList<K, V>::put(const K &key, const V &value) {
    /**
     * @brief 插入或更新键值对
     * @param key：要插入或更新的键
     * @param value：要插入或更新的值
     * @return WriteResult：表示写入结果的枚举值，可能是 Inserted、Updated 或 Failed
     * @details 如果键不存在，则插入新的键值对并返回 Inserted；如果键已存在，则更新其值并返回 Updated；如果发生错误，则返回 Failed。
     * @note 先追加 WAL，若 sync_wal = true，则执行 flush + fsync 确保数据写入磁盘，然后再修改内存中的跳表结构。这样即使在修改内存之前发生崩溃，也能通过 WAL 恢复数据，保证数据的持久性和一致性。
     */
    std::unique_lock<std::shared_mutex> lock(mtx_);
    // 先追加 WAL，确保数据持久化
    if (_enable_wal) {
        // 永远使用 put 操作的 WAL 记录格式，即使是更新操作，也使用 put 记录，这样在恢复时不需要区分插入和更新，简化恢复逻辑。
        if (!append_put_wal_unlocked(key, value)) {
            throw std::runtime_error("failed to append WAL record for key: " + std::to_string(key));
        }
    }
    return put_unlocked(key, value, true);
}

template <typename K, typename V>
void SkipList<K, V>::dump_file_unlocked() {
    _file_writer.open(_store_file, std::ios::out | std::ios::trunc);
    if (!_file_writer.is_open()) {
        throw std::runtime_error("failed to open snapshot file for writing: " + _store_file);
    }

    _file_writer << kSnapshotHeader << '\n';
    Node<K, V> *node = this->_header->forward[0];
    while (node != nullptr) {
        _file_writer << encode_snapshot_line(node->get_key(), node->get_value()) << '\n';
        node = node->forward[0];
    }

    _file_writer.flush();
    if (!_file_writer) {
        _file_writer.close();
        throw std::runtime_error("failed to flush snapshot file: " + _store_file);
    }
    _file_writer.close();
}

template <typename K, typename V>
void SkipList<K, V>::load_file_unlocked() {
    _file_reader.open(_store_file);
    if (!_file_reader.is_open()) {
        return;
    }

    std::string line;
    if (!std::getline(_file_reader, line) || line != kSnapshotHeader) {
        _file_reader.close();
        throw std::runtime_error("invalid snapshot header in: " + _store_file);
    }

    while (std::getline(_file_reader, line)) {
        int key = 0;
        std::string escaped_value;
        std::string value;

        if (!decode_snapshot_line(line, &key, &escaped_value)) {
            _file_reader.close();
            throw std::runtime_error("invalid snapshot record in: " + _store_file);
        }
        if (!unescape_value(escaped_value, &value)) {
            _file_reader.close();
            throw std::runtime_error("invalid escaped snapshot value in: " + _store_file);
        }
        put_unlocked(key, value, false);
    }
    _file_reader.close();
}

template <>
bool SkipList<int, std::string>::parse_int_key(const std::string &str, int *key) {
    if (str.empty()) {
        return false;
    }

    try {
        std::size_t parsed = 0;
        const int value = std::stoi(str, &parsed);
        if (parsed != str.size()) {
            return false;
        }
        *key = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

template <>
std::string SkipList<int, std::string>::encode_snapshot_line(int key, const std::string &value) {
    return std::to_string(key) + kFieldSep + escape_value(value);
}

template <>
bool SkipList<int, std::string>::decode_snapshot_line(const std::string &line,
                                                      int *key,
                                                      std::string *value) {
    /**
     * @brief 解码 snapshot 文件中的一行内容，提取键值对
     * @param line：待解码的字符串行，格式为 "key\tvalue"
     * @param key：存储解析出的键的指针
     * @param value：存储解析出的值的指针
     * @return bool：如果解码成功返回 true，否则返回 false
     * @details 解析字符串中的整数键和字符串值，确保正确处理分隔符和转义字符，提取出原始的键值对。
     */
    const std::size_t pos = line.find(kFieldSep);
    if (pos == std::string::npos) {
        return false;
    }
    if (!parse_int_key(line.substr(0, pos), key)) {
        return false;
    }
    *value = line.substr(pos + 1);
    return true;
}

template <>
std::string SkipList<int, std::string>::encode_wal_put_record(int key, const std::string &value) {
    std::string record;
    record.push_back(kWalPutOp);
    record.push_back(kFieldSep);
    record += std::to_string(key);
    record.push_back(kFieldSep);
    record += escape_value(value);
    return record;
}

template <>
std::string SkipList<int, std::string>::encode_wal_delete_record(int key) {
    std::string record;
    record.push_back(kWalDeleteOp);
    record.push_back(kFieldSep);
    record += std::to_string(key);
    return record;
}

template <>
bool SkipList<int, std::string>::decode_wal_record(const std::string &line, char *op_type, int *key, std::string *value) {
    if (line.size() < 3 || line[1] != kFieldSep) {
        return false;
    }

    *op_type = line[0];
    if (*op_type == kWalDeleteOp) {
        value->clear();
        return parse_int_key(line.substr(2), key);
    }

    if (*op_type != kWalPutOp) {
        return false;
    }

    const std::size_t pos = line.find(kFieldSep, 2);
    if (pos == std::string::npos) {
        return false;
    }
    if (!parse_int_key(line.substr(2, pos - 2), key)) {
        return false;
    }
    *value = line.substr(pos + 1);
    return true;
}

template <> bool SkipList<int, std::string>::append_wal_line_unlocked(const std::string &line) {
    /**
     * @brief 追加 WAL 日志记录，按行追加到 WAL 文件中
     * @param line：要追加的 WAL 日志记录，格式为 "op_type\tkey\tvalue" 或 "op_type\tkey"
     * @return bool：如果追加成功返回 true，否则返回 false
     */

    const std::string record = line + '\n';
    const int fd = ::open(_wal_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);

    // 使用 RAII 原则管理文件描述符，确保在函数退出时能够正确关闭文件，避免资源泄漏。
    if (fd == -1) {
        return false;
    }
    size_t total_written = 0;
    while (total_written < record.size()) {
        const ssize_t written = ::write(fd, record.data() + total_written, record.size() - total_written);
        if (written == -1) {
            if (errno == EINTR) {
                continue; // 如果写入被信号中断，继续尝试写入
            }
            // 最终失败，关闭文件描述符并返回 false
            ::close(fd);
            return false;
        }
        total_written += static_cast<size_t>(written);
    }
    // 写入完毕，如果 sync_wal 配置为 true，则执行 fsync 确保数据写入磁盘，增强数据的持久性。
    if (_sync_wal && ::fsync(fd) == -1) {
        // 若 fsync 失败，关闭文件描述符并返回 false
        ::close(fd);
        return false;
    }
    // 关闭文件描述符并返回 true，表示追加成功
    return ::close(fd) == 0;
}

template <typename K, typename V>
bool SkipList<K, V>::append_put_wal_unlocked(const K &key, const V &value) {
    /**
     * @brief 追加 WAL 日志记录，按行追加到 WAL 文件中
     * @param key：要追加的键
     * @param value：要追加的值
     * @return bool：如果追加成功返回 true，否则返回 false。
     * @details 即为对应的 put 操作生成 WAL 日志记录，格式为 "P\tkey\tvalue"，并调用 append_wal_line_unlocked 方法将日志记录追加到 WAL 文件中。
     */
    return append_wal_line_unlocked(encode_wal_put_record(key, value));
}

template <typename K, typename V>
bool SkipList<K, V>::append_delete_wal_unlocked(const K &key) {
    /**
     * @brief 追加 WAL 日志记录，按行追加到 WAL 文件中
     * @param key：要追加的键
     * @return bool：如果追加成功返回 true，否则返回 false。
     * @details 即为对应的 delete 操作生成 WAL 日志记录，格式为 "D\tkey"，并调用 append_wal_line_unlocked 方法将日志记录追加到 WAL 文件中。
     */
    return append_wal_line_unlocked(encode_wal_delete_record(key));
}

template <typename K, typename V>
bool SkipList<K, V>::replay_wal_unlocked() {
    /**
     * @brief 回放 WAL 日志，恢复数据
     * @return bool：如果回放成功返回 true，否则返回 false
     * @details 从 WAL 文件中逐行读取日志记录，解析每条记录的操作类型、键和值，并根据操作类型执行相应的插入或删除操作来恢复数据。
     */
    std::ifstream wal_file(_wal_file);
    if (!wal_file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(wal_file, line)) {
        char op_type;
        int key;
        std::string escaped_value;

        if (!decode_wal_record(line, &op_type, &key, &escaped_value)) {
            wal_file.close();
            return false; // 如果解析 WAL 记录失败，则返回 false，表示回放失败。
        }
        if (op_type == kWalPutOp) {
            std::string value;
            // 需要对原始 WAL 记录中的转义值进行反转义，恢复出原始的值。如果反转义失败，则返回 false，表示回放失败。
            if (!unescape_value(escaped_value, &value)) {
                wal_file.close();
                return false;
            }
            put_unlocked(key, value, true);
        } else if (op_type == kWalDeleteOp) {
            erase_unlocked(key);
        } else {
            wal_file.close();
            return false; // 如果遇到未知的 WAL 操作类型，则返回 false，表示回放失败。
        }
    }
    wal_file.close();
    return true;
}

} // namespace skiplist
