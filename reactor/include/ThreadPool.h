#pragma once
#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

class ThreadPool {
public:
    using Task = std::function<void()>;

    ThreadPool(size_t numThreads);
    ~ThreadPool();

    void run(Task task);//提交任务，非阻塞

private:
    void workerLoop(); //每个工作的线程
    std::vector<std::thread> threads_;// 线程池中的线程
    std::queue<Task> tasks_;    // 任务队列
    std::mutex mutex_;  // 互斥锁，用于保护任务队列
    std::condition_variable cond_; // 条件变量，用于线程间的通知
    bool running_ = true;
};