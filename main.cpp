/* ************************************************************************
> File Name:     main.cpp
> Author:        程序员Carl
> 微信公众号:    代码随想录
> Created Time:  Sun Dec  2 20:21:41 2018
> Description:   
 ************************************************************************/
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "skiplist.h"

using namespace skiplist;

int main() {
    SkipListOptions options(6);
    options.enable_wal = true;
    options.sync_wal = true;

    SkipList<int, std::string> primary(options);
    primary.put(1, "刘备");
    primary.put(3, "关羽");
    primary.put(7, "张飞");
    primary.put(8, "赵云");
    primary.put(9, "诸葛亮");

    if (const auto value = primary.get(9); value.has_value()) {
        std::cout << "get(9): " << value.value() << '\n';
    }

    const std::vector<std::pair<int, std::string>> scan_result = primary.scan(3, 10);
    std::cout << "scan[3, 10):";
    for (const auto& [key, value] : scan_result) {
        std::cout << ' ' << key << '=' << value;
    }
    std::cout << '\n';

    if (!primary.checkpoint()) {
        std::cerr << "checkpoint failed\n";
        return 1;
    }

    primary.put(9, "司马懿");
    primary.put(11, "曹操");
    primary.erase(3);

    SkipList<int, std::string> recovered(options);
    if (!recovered.recover()) {
        std::cerr << "recover failed\n";
        return 1;
    }

    std::cout << "recovered size: " << recovered.size() << '\n';
    recovered.display_list();
    return 0;
}
