#include "TimerQueue.h"
#include "EventLoop.h"
#include <time.h>
#include <sys/timerfd.h>
#include <unistd.h>

// 构造函数：
//   1. 创建 timerfd — 一个由内核管理的定时器文件描述符
//   2. 用 Channel 包装 timerfd，加入 epoll 监听 EPOLLIN
//   3. 注册回调：当 timerfd 可读时 → handleRead()
TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerChannel_(
          timerfd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC),
          loop_)
      // CLOCK_MONOTONIC: 单调时钟，不受系统时间调整影响
      // TFD_NONBLOCK:    非阻塞
      // TFD_CLOEXEC:     exec() 时自动关闭此 fd
{
    // timerfd 到期变为可读 → 回调 handleRead
    timerChannel_.setReadCallback([this]() {
        handleRead();
    });
    timerChannel_.enableReading();  // 注册到 epoll
}

// 析构：关闭 timerfd（Channel 析构会自动从 epoll 移除）
TimerQueue::~TimerQueue()
{
    close(timerfd_);
}

// 添加定时器
//   1. 生成唯一 ID
//   2. 检查新 timer 是否比所有现有 timer 更早到期（earliestChanged）
//   3. 插入 timers_ map
//   4. 如果 earliestChanged，重置 timerfd 的唤醒时间
int64_t TimerQueue::addTimer(Timer::TimerCallback cb, int64_t expiration, double interval)
{
    // timers_ 为空 或 新 timer 比 begin() 更早到期 → 需要更新 timerfd
    bool earliestChanged = (timers_.empty() || expiration < timers_.begin()->first);

    // 插入 map，key=过期时间，value=Timer 对象
    Timer timer(std::move(cb), expiration, interval);
    timer.setId(nextTimerId_++);
    timers_.insert({expiration, timer});

    if (earliestChanged) {
        resetTimerfd(expiration);  // 让 timerfd 在新最早时间触发
    }
    id2exp_[timer.id()] = expiration; // 记录 timerId 对应的 expiration

    return timer.id();
}

// TODO: 按 timerId 查找并移除 timer
void TimerQueue::cancel(int64_t timerId)
{
    auto it = id2exp_.find(timerId);
    if (it != id2exp_.end()) {
        timers_.erase(it->second);  // 按 expiration 从 timers_ 删
        id2exp_.erase(it);
    }
}

// timerfd 到期时的回调（由 epoll 驱动）
//   1. 读 timerfd — 必须读，否则下一次 epoll_wait 立即再次触发
//   2. 获取当前时间（微秒）
//   3. 遍历 timers_，把 expiration <= now 的 timer 全部取出执行
//   4. 重复 timer → restart 后重新插入 map
//   5. 如果还有剩余 timer → resetTimerfd 设置下一次唤醒
void TimerQueue::handleRead()
{
    // 消费 timerfd 事件（读出的值是该 timerfd 自创建以来的到期次数）
    uint64_t exp;
    read(timerfd_, &exp, sizeof(exp));

    // 获取当前单调时间（微秒）
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t now = ts.tv_sec * 1'000'000 + ts.tv_nsec / 1'000;


    // 处理所有到期的 timer
     // 第一步：从 map 里拿出所有到期的 timer（它们已不属于 timers_）
    std::vector<Timer> expired;
    for (auto it = timers_.begin(); it != timers_.end(); ) {
        if (it->first > now) break;
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

// 用 timerfd_settime 设置 timerfd 的下次到期时间
// earliestExpiration: 绝对微秒时间戳
// TFD_TIMER_ABSTIME 表示使用绝对时间（相对于 CLOCK_MONOTONIC 的 0 点）
void TimerQueue::resetTimerfd(int64_t earliestExpiration)
{
    struct itimerspec newValue = {};  // 零初始化（it_interval = {0,0}，= 单次触发）

    int64_t microSec = earliestExpiration;

    // 微秒 → 秒 + 纳秒
    newValue.it_value.tv_sec  = microSec / 1'000'000;
    newValue.it_value.tv_nsec = (microSec % 1'000'000) * 1'000;

    // TFD_TIMER_ABSTIME: 按绝对时间触发（不会累积系统休眠时间）
    // 如果 earliestExpiration 已经是过去的时间，内核会立即触发 timerfd
    timerfd_settime(timerfd_, TFD_TIMER_ABSTIME, &newValue, nullptr);
}