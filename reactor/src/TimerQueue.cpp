#include "TimerQueue.h"
#include "EventLoop.h"
#include <time.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <cassert>

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerChannel_(
          timerfd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC),
          loop_)
{
    timerChannel_.setReadCallback([this]() {
        handleRead();
    });
    timerChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
    close(timerfd_);
}

int64_t TimerQueue::addTimer(Timer::TimerCallback cb, int64_t expiration, double interval)
{
    // TimerQueue 与 EventLoop 绑定，必须在所属 IO 线程调用
    assert(loop_->isInLoopThread());

    bool earliestChanged = (timers_.empty() || expiration < timers_.begin()->first);

    Timer timer(std::move(cb), expiration, interval);
    timer.setId(nextTimerId_++);
    timers_.insert({expiration, timer});

    if (earliestChanged) {
        resetTimerfd(expiration);
    }
    id2exp_[timer.id()] = expiration;

    return timer.id();
}

void TimerQueue::cancel(int64_t timerId)
{
    assert(loop_->isInLoopThread());

    auto it = id2exp_.find(timerId);
    if (it != id2exp_.end()) {
        // 在 multimap 中查找并精确删除：同 expiration 下可能有多个 timer，
        // 必须匹配 timerId 才能避免误删
        auto range = timers_.equal_range(it->second);
        for (auto ti = range.first; ti != range.second; ++ti) {
            if (ti->second.id() == timerId) {
                timers_.erase(ti);
                break;
            }
        }
        id2exp_.erase(it);
    }
}

void TimerQueue::handleRead()
{
    uint64_t exp;
    read(timerfd_, &exp, sizeof(exp));

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t now = ts.tv_sec * 1'000'000 + ts.tv_nsec / 1'000;

    // 第一步：从 map 里拿出所有到期的 timer，同时清理 id2exp_
    std::vector<Timer> expired;
    for (auto it = timers_.begin(); it != timers_.end(); ) {
        if (it->first > now) break;
        id2exp_.erase(it->second.id());   // 修复：同步清理 id2exp_，
                                           // 防止回调中 cancel 已取出的 timer
        expired.push_back(std::move(it->second));
        it = timers_.erase(it);
    }

    // 第二步：执行回调（此时 cancel 扫不到它们，安全）
    for (auto& timer : expired) {
        timer.run();
        if (timer.repeat()) {
            timer.restart(now);
            timers_.insert({timer.expiration(), std::move(timer)});
        }
    }

    if (!timers_.empty()) resetTimerfd(timers_.begin()->first);
}

void TimerQueue::resetTimerfd(int64_t earliestExpiration)
{
    struct itimerspec newValue = {};

    int64_t microSec = earliestExpiration;

    newValue.it_value.tv_sec  = microSec / 1'000'000;
    newValue.it_value.tv_nsec = (microSec % 1'000'000) * 1'000;

    timerfd_settime(timerfd_, TFD_TIMER_ABSTIME, &newValue, nullptr);
}