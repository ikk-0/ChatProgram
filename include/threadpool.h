#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    // 构造函数：创建指定数量的工作线程
    explicit ThreadPool(size_t threads = 4);
    
    // 析构函数：停止所有线程
    ~ThreadPool();
    
    // 提交任务（线程安全）
    void submit(std::function<void()> task);
    
    // 停止线程池
    void stop();

private:
    void workerLoop();  // 工作线程的主循环
    
    std::vector<std::thread> workers;           // 工作线程列表
    std::queue<std::function<void()>> tasks;    // 任务队列
    std::mutex queue_mutex;                     // 保护任务队列的锁
    std::condition_variable condition;          // 条件变量，用于唤醒线程
    std::atomic<bool> running;                  // 是否运行中
};

#endif