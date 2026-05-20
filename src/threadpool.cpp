#include "threadpool.h"
ThreadPool::ThreadPool(size_t threads) :running(true)
{
    for(size_t i=0;i<threads;i++)
    {
        workers.emplace_back([this]{workerLoop();});
        // C++ lambda 表达式:创建一个新线程，这个线程一启动就去跑线程池里的 workerLoop () 函数
    }
}

ThreadPool::~ThreadPool()
{
    stop();
}

void ThreadPool::submit(std::function<void()> task)
{
    if(!running) return;
    //上锁作用域
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        tasks.push(std::move(task));//直接把任务的所有权直接转移给队列，不复制，迅速
    }// 离开作用域,锁自动销毁
    condition.notify_one();// 唤醒一个空闲线程
}

void ThreadPool::stop()
{
    if(!running) return;
    running = false;
    condition.notify_all();

    for(auto& worker : workers)
    {
        if(worker.joinable())//检查线程能不能等它结束（可能已经join过了）
        {
            worker.join();//等待线程接受
        }
    }
}

void ThreadPool::workerLoop() {
    while (running) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            // 等待任务到来，或者线程池停止
            condition.wait(lock, [this] {
                return !tasks.empty() || !running;
            });
            
            // 如果停止且没有任务，退出
            if (!running && tasks.empty()) {
                return;
            }
            
            // 取出任务
            task = std::move(tasks.front());
            tasks.pop();
        }
        
        // 执行任务（不持有锁）
        if (task) {
            task();
        }
    }
}