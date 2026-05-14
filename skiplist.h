/* ************************************************************************
> File Name:     skiplist.h
> Author:        程序员Carl
> 微信公众号:    代码随想录
> Created Time:  Sun Dec  2 19:04:26 2018
> Description:   
 ************************************************************************/
#pragma once

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
#include <type_traits>
#include <cstdint>

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
    void get_key_value_from_string(const std::string& str, std::string* key, std::string* value); // 从字符串中提取键值对
    bool is_valid_string(const std::string& str); // 判断字符串是否有效，是否包含键值对分隔符

private:
    // 定义键值对分隔符，静态常量成员，所有 SkipList 实例共享同一个分隔符
    static constexpr const char* delimiter = ":";

    // 定义转义函数，用于处理键值对中可能包含的分隔符，避免在解析时出现问题
    std::string escape_value(const std::string &value);
    // 定义反转义函数，用于将转义后的字符串还原为原始值，确保在加载数据时能够正确解析出键值对。
    std::string unescape_value(const std::string &value);

    // 删除所有节点，替代原始的 clear 函数，避免递归删除导致的栈溢出问题。
    void clear_all_nodes();
    
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
    return put_unlocked(key, value, false) == WriteResult::inserted ? 0 : 1;
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
     * @details 将跳表中的所有元素写入到指定的文件中，格式为 "key:value"，每个键值对占一行。
     * @note 每次调用 dump 进行全量持久化。而且在 dump 之前或过程中发生崩溃，则所有数据都会丢失，后续改进可以使用 WAL 日志来实现增量持久化，减少数据丢失的风险。
     * @note 当前持久化数据格式比较脆弱，如果 value 中包含了分隔符 ":"，则在加载数据时会出现问题，后续改进可以使用更健壮的序列化方式来存储数据，例如 JSON 或者 Protocol Buffers。
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

template <typename K, typename V>
void SkipList<K, V>::get_key_value_from_string(const std::string &str,
                                               std::string *key,
                                               std::string *value) {
    /**
     * @brief 根据字符串解析出键值对
     * @param str：待解析的字符串
     * @param key：存储解析出的键
     * @param value：存储解析出的值
     */

    if(!is_valid_string(str)) {
        return;
    }
    *key = str.substr(0, str.find(delimiter));
    *value = str.substr(str.find(delimiter) + 1, str.length());
}

template<typename K, typename V>
bool SkipList<K, V>::is_valid_string(const std::string& str) {
    /**
     * @brief 判断是否是一个有效的键值对字符串
     * @param str：待判断的字符串
     * @return bool：如果字符串非空且包含分隔符，则返回 true；否则返回 false
     */
    if (str.empty()) {
        return false;
    }
    if (str.find(delimiter) == std::string::npos) {
        return false;
    }
    return true;
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
     * @brief 转义字符串中的分隔符
     * @param value：待转义的字符串
     * @return 转义后的字符串
     * @details 将字符串中的分隔符 ":" 替换为 "\:"，以避免在解析键值对时出现问题。
     */
    std::string escaped_value = value;
    size_t pos = 0;
    std::string escaped_delimiter = std::string("\\") + delimiter;
    while ((pos = escaped_value.find(delimiter, pos)) != std::string::npos) {
        escaped_value.replace(pos, 1, escaped_delimiter);
        pos += escaped_delimiter.length(); // 跳过转义后的分隔符
    }
    return escaped_value;
}

template<typename K, typename V>
std::string SkipList<K, V>::unescape_value(const std::string &value) {
    /**
     * @brief 反转义字符串中的分隔符
     * @param value：待反转义的字符串
     * @return 反转义后的字符串
     * @details 将字符串中的转义分隔符 "\:" 替换回 ":"，以还原原始值。
     */
    std::string unescaped_value = value;
    size_t pos = 0;
    std::string escaped_delimiter = std::string("\\") + delimiter;
    while ((pos = unescaped_value.find(escaped_delimiter, pos)) != std::string::npos) {
        unescaped_value.replace(pos, escaped_delimiter.length(), delimiter);
        pos += std::string(delimiter).length(); // 跳过分隔符
    }
    return unescaped_value;
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
     * @details 通过调用 dump_file 方法将跳表中的数据持久化到文件中，创建一个检查点。
     * @note 目前的实现是全量持久化，如果在 dump 之前或过程中发生崩溃，则所有数据都会丢失。后续改进可以使用 WAL 日志来实现增量持久化，减少数据丢失的风险。
     */
    // 检查约束条件，确保当前 SkipList 的键值类型支持持久化。
    static_assert(is_persistence_supported_v<K, V>, "Current key-value types do not support persistence in checkpoint.");
    // 将当前跳表的状态持久化到文件中，创建检查点。需要写锁，即独占锁，确保在创建检查点时跳表不会被修改，保持数据一致性。
    std::unique_lock<std::shared_mutex> lock(mtx_);
    // 创建检查点，通过调用 dump_file 方法将跳表中的数据持久化到文件中。
    // 如果 dump_file 成功完成，则返回 true；如果发生异常或者失败，则返回 false。
    try {
        dump_file_unlocked();
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
     * @note 目前的实现是全量恢复，如果在 load_file 之前或过程中发生崩溃，则无法恢复数据。后续改进可以使用 WAL 日志来实现增量恢复，减少数据丢失的风险。
     */
    // 检查约束条件，确保当前 SkipList 的键值类型支持持久化。
    static_assert(is_persistence_supported_v<K, V>, "Current key-value types do not support persistence in recover.");
    std::unique_lock<std::shared_mutex> lock(mtx_);
    // 从检查点恢复，通过调用 load_file 方法从持久化文件中加载数据，恢复跳表的状态。
    // 如果 load_file 成功完成，则返回 true；如果发生异常或者失败，则返回 false。
    try {
        load_file_unlocked();
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

template <typename K, typename V>
WriteResult SkipList<K, V>::put(const K &key, const V &value) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    return put_unlocked(key, value, true);
}

template <typename K, typename V>
void SkipList<K, V>::dump_file_unlocked() {
    _file_writer.open(_store_file);
    Node<K, V> *node = this->_header->forward[0];
    while (node != nullptr) {
        _file_writer << node->get_key() << ":" << node->get_value() << "\n";
        node = node->forward[0];
    }
    _file_writer.flush();
    _file_writer.close();
}

template <typename K, typename V>
void SkipList<K, V>::load_file_unlocked() {
    _file_reader.open(_store_file);
    std::string line;
    std::string key;
    std::string value;
    while (getline(_file_reader, line)) {
        key.clear();
        value.clear();
        get_key_value_from_string(line, &key, &value);
        if (key.empty() || value.empty()) {
            continue;
        }
        put_unlocked(stoi(key), value, false);
    }
    _file_reader.close();
}

} // namespace skiplist
