/* ************************************************************************
> File Name:     stress_test.cpp
> Author:        程序员Carl
> 微信公众号:    代码随想录
> Created Time:  Sun 16 Dec 2018 11:56:04 AM CST
> Description: 缺少不同线程数的性能对比，后续改进可以增加不同线程数的测试来比较性能差异。没有同样操作数量的 std::map 作为基准测试，后续改进可以增加 std::map 的性能测试来比较跳表和 std::map 的性能差异。
 ************************************************************************/

#include <iostream>
#include <chrono>
#include <cstdlib>
#include <pthread.h>
#include <time.h>
#include "../skiplist.h"

using namespace skiplist;

#define NUM_THREADS 4 // 实际只有单线程，后续改进可以增加线程数量来测试并发性能。
#define TEST_COUNT 100000
SkipList<int, std::string> skipList(18);

void *insertElement(void* threadid) {
    long tid = *static_cast<long*>(threadid);
    std::cout << tid << '\n';  
    int tmp = TEST_COUNT/NUM_THREADS; 
	for (long i = tid * tmp, count = 0; count < tmp; i++) {
        count++;
		skipList.insert_element(rand() % TEST_COUNT, "a"); 
	}
    pthread_exit(nullptr);
}

void *getElement(void* threadid) {
    long tid = *static_cast<long*>(threadid);
    std::cout << tid << '\n';  
    int tmp = TEST_COUNT/NUM_THREADS; 
	for (long i = tid * tmp, count = 0; count < tmp; i++) {
        count++;
		skipList.search_element(rand() % TEST_COUNT); 
	}
    pthread_exit(nullptr);
}

int main() {
    srand (time(nullptr));  
    {

        pthread_t threads[NUM_THREADS];
        int rc;

        auto start = std::chrono::high_resolution_clock::now();

        for(int i = 0; i < NUM_THREADS; i++ ) {
            std::cout << "main() : creating thread, " << i << '\n';
            rc = pthread_create(&threads[i], nullptr, insertElement, &threads[i]);

            if (rc) {
                std::cout << "Error:unable to create thread," << rc << '\n';
                exit(-1);
            }
        }

        void *ret; // 线程返回值
        for(int i = 0; i < NUM_THREADS; i++ ) {
            if (pthread_join(threads[i], &ret) !=0 )  {
                perror("pthread_create() error"); 
                exit(3);
            }
        }
        auto finish = std::chrono::high_resolution_clock::now(); 
        std::chrono::duration<double> elapsed = finish - start;
        std::cout << "insert elapsed:" << elapsed.count() << '\n';
    }
    // 没有测试查询性能和删除性能，后续改进可以增加查询和删除的线程来测试并发性能。
    // skipList.displayList();

    // {
    //     pthread_t threads[NUM_THREADS];
    //     int rc;
    //     int i;
    //     auto start = std::chrono::high_resolution_clock::now();

    //     for( i = 0; i < NUM_THREADS; i++ ) {
    //         std::cout << "main() : creating thread, " << i << std::endl;
    //         rc = pthread_create(&threads[i], NULL, getElement, (void *)i);

    //         if (rc) {
    //             std::cout << "Error:unable to create thread," << rc << std::endl;
    //             exit(-1);
    //         }
    //     }

    //     void *ret;
    //     for( i = 0; i < NUM_THREADS; i++ ) {
    //         if (pthread_join(threads[i], &ret) !=0 )  {
    //             perror("pthread_create() error"); 
    //             exit(3);
    //         }
    //     }

    //     auto finish = std::chrono::high_resolution_clock::now(); 
    //     std::chrono::duration<double> elapsed = finish - start;
    //     std::cout << "get elapsed:" << elapsed.count() << '\n';
    // }

	pthread_exit(nullptr);
    return 0;

}
