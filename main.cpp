/* ************************************************************************
> File Name:     main.cpp
> Author:        程序员Carl
> 微信公众号:    代码随想录
> Created Time:  Sun Dec  2 20:21:41 2018
> Description:   
 ************************************************************************/
#include <iostream>
#include "skiplist.h"
#define FILE_PATH "./store/dumpFile"
using namespace skiplist;

int main() {

    // 键值中的key用int型，如果用其他类型，需要自定义比较函数
    // 而且如果修改key的类型，同时需要修改skipList.load_file函数
    SkipList<int, std::string> skipList(6);
    skipList.put(1, "刘备");
    skipList.put(3, "关羽");
    skipList.put(7, "张飞");
    skipList.put(8, "赵云");
    skipList.put(9, "诸葛亮");
    skipList.put(19, "晋");
    skipList.put(19, "司马懿");

    const SkipList<int, std::string>& read_view = skipList;

    std::cout << "skipList size:" << read_view.size() << '\n';

    skipList.dump_file();

    // skipList.load_file();

    std::cout << "contains 9: " << read_view.contains(9) << '\n';
    std::cout << "contains 18: " << read_view.contains(18) << '\n';


    skipList.display_list();

    skipList.erase(3);
    skipList.erase(7);

    std::cout << "skipList size:" << read_view.size() << '\n';

    skipList.display_list();
}
