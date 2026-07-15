#include "ThreadPool.h"
#include <pthread.h>

ThreadPool::ThreadPool(size_t numThreads)
{
    for(size_t i = 0; i < numThreads; i++){
        threads_.emplace_back([this] {
            pthread_setschedparam(pthread_self(), SCHED_OTHER, nullptr);
            workerLoop();
        });
    }
}

ThreadPool::~ThreadPool()
{
    {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    }
    cond_.notify_all();
    for (auto& t : threads_) t.join();

}

void ThreadPool::run(Task task)//提交任务，非阻塞
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));
    }
    cond_.notify_one(); // 唤醒一个等待的线程
}

void ThreadPool::workerLoop() //每个工作的线程
{
    while(running_){
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_.wait(lock, [this] { return !tasks_.empty() || !running_;});// 等待任务队列非空或线程池停止
            if (!running_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();  // 在锁外执行，不阻塞其他线程取任务
    }
}