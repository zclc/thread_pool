#include <stdio.h>
#include <cmath>
#include <chrono>
#include <thread>
#include <iostream>

#include "thread_pool.h"

const int MIN = 1;
const int MAX = 10e6;
// 计算 [min, max] 范围内 平方根和
double work(int min, int max)
{
    double res = 0;

    for (int i = min; i <= max; i++)
    {
        res+=sqrt(i);
    }
    
    return res;
}

int main(int argc, char const *argv[])
{
    // 获取系统支持线程数
    unsigned concurrent_count = std::thread::hardware_concurrency();

    // 创建线程池
    ThreadPool tp(concurrent_count);

    // 存放每个work线程返回的 future对象
    std::vector<std::future<double>> res_future(concurrent_count);

    // 单线程调用 work计算 1-10e6 平方根和 
    auto start_time = std::chrono::steady_clock::now();
    double res_serial = work(MIN, MAX);

    auto end_time = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time).count();
    std::cout << "one thread : " << res_serial << ", time: " << ms << "ms" << std::endl; 

    // 多线程, 将[min, max] 划分范围分给每个线程计算平方根和
    start_time = std::chrono::steady_clock::now();
    for (int i = 0; i < concurrent_count; i++)
    {   
       res_future[i] = tp.submit(work, (MAX-MIN+1)/concurrent_count * i, (MAX-MIN+1)/concurrent_count * (i+1) - 1);
    }
    
    double res = 0.0;

    for (int i = 0; i < concurrent_count; i++)
    {
        res += res_future[i].get();
    }
    
    end_time = std::chrono::steady_clock::now();
    ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time).count();
    std::cout << "multi thread : " << res << ", time: " << ms << "ms" << std::endl; 

    tp.shutdow();

    return 0;
}
