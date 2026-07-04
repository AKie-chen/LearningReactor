# TinyReactor — 从 epoll echo 到多线程 Reactor HTTP 服务器

基于 Linux epoll ET 模式从零构建的 C++ 高性能 HTTP 服务器，逐步演进，每个 commit 对应一个可编译运行的中间状态。

## 架构

```
main
├── ThreadPool (4 线程, CPU 密集任务)
├── EventLoop (主线程, 只 accept)
│   ├── TimerQueue (timerfd + eventfd, 连接超时管理)
│   ├── TcpServer
│   │   └── Acceptor (SO_REUSEPORT)
│   └── EventLoopThread × N (sub loops, 连接 I/O)
└── TcpConnection (自包含: HttpContext + timerId + Channel + Buffer)
```

### 数据流 (一次 HTTP 请求)

```
epoll_wait → Channel::handleEvent
  → TcpConnection::handleRead
    → Buffer::readFd (readv, ET 模式)
      → HttpContext::parseRequest (状态机)
        → ThreadPool::run (工作线程构造 HttpResponse)
          → EventLoop::queueInLoop (切回 I/O 线程)
            → TcpConnection::send → ::send
```

## 特性

| 类别 | 内容 |
|------|------|
| I/O 模型 | epoll ET, 非阻塞 I/O |
| 缓冲区 | readv/vwritev, prependable 三区模型, 自动扩容/缩容 |
| HTTP | GET/POST 解析, Content-Length, Keep-Alive, 状态机 |
| 定时器 | timerfd + CLOCK_MONOTONIC, O(log n) cancel, 空闲超时断开 |
| 多线程 | 主从 Reactor + 线程池, eventfd 跨线程唤醒 |
| 协议 | HTTP/1.1 (最小子集, 支持 curl/ab/wrk) |

## 快速开始

### 环境要求

- Linux (kernel ≥ 2.6.27, 需要 `timerfd_create` / `eventfd`)
- CMake ≥ 3.10
- GCC ≥ 8 或 Clang ≥ 7 (C++17)
- pthread

### 编译

```bash
cd reactor
mkdir -p build && cd build
cmake ..
make
```

### 运行

```bash
./main
# 服务监听 0.0.0.0:8080
```

### 测试

```bash
# 功能验证
curl -v http://localhost:8080/

# 压力测试
ab -n 10000 -c 100 http://127.0.0.1:8080/
wrk -t4 -c100 -d30s http://127.0.0.1:8080/
```

## 性能

测试环境: Linux 4.18, 单机回环, 响应 `Hello, World!\n`

| 场景 | 吞吐量 | 延迟 P50 | 延迟 P99 |
|------|--------|----------|----------|
| 100 连接 Keep-Alive × 100 | **48,156 req/s** | 0.37 ms | 1.23 ms |
| 200 连接 × 10 请求 | 18,984 req/s | — | — |
| 50 连接 × 20 请求 | 21,896 req/s | — | — |
| 1000 短连接 × 1 请求 | 892 req/s | — | — |
| 单次请求 | — | 0.75 ms | 1.34 ms |

> 短连接吞吐受 TCP 握手开销影响, 非服务器瓶颈。

## 演进路线 (8 Tasks)

| Task | 内容 | 关键产出 |
|------|------|----------|
| 1 | EventLoop + Channel | epoll 事件抽象, `added_` 标志 ADD/MOD 管理 |
| 2 | Buffer | readv 双缓冲, 三区模型, peek/retrieve |
| 3 | TcpConnection | fd 生命周期封装, queueInLoop 安全销毁 |
| 4 | Acceptor + TcpServer | listen/accept 封装, 两阶段构造 |
| 5 | HTTP 协议 | 请求行/头部/Body 状态机, 响应序列化 |
| 6 | 定时器系统 | timerfd, 空闲超时断开, cancel O(log n) |
| 7 | 线程池 | CPU 任务卸载, eventfd 跨线程回调 |
| 8 | Multi-Reactor | 主从 epoll, 轮询分发, per-loop TimerQueue |

## 项目结构

```
reactor/
├── include/
│   ├── Acceptor.h          # listenfd 封装, accept 循环
│   ├── Buffer.h            # 非连续缓冲区 (readv/vwritev)
│   ├── Channel.h           # fd + events + callbacks 抽象
│   ├── EventLoop.h         # epoll 事件循环
│   ├── EventLoopThread.h   # EventLoop + thread 绑定
│   ├── HttpContext.h       # HTTP 请求解析状态机
│   ├── HttpRequest.h       # HTTP 请求数据结构
│   ├── HttpResponse.h      # HTTP 响应序列化
│   ├── TcpConnection.h     # 连接生命周期管理
│   ├── TcpServer.h         # 服务器入口
│   ├── ThreadPool.h        # 工作线程池
│   ├── Timer.h             # 定时器对象
│   └── TimerQueue.h        # timerfd 定时器队列
├── src/
│   ├── main.cpp            # 入口 (20 行)
│   └── *.cpp               # 各模块实现
└── CMakeLists.txt
```

## 关键设计决策

### 为什么用 ET 而不是 LT？
ET 模式下每个事件只通知一次，必须循环读到 EAGAIN。好处是减少 epoll_wait 返回次数，缺点是实现更复杂。Buffer 的 readv + extrabuf 正是为此设计——一次读尽可能多的数据。

### 为什么延迟销毁 (queueInLoop)？
事件回调中不能 `delete this`——当前还在 `epoll_wait` 的 for 循环里，Channel 指针被 `events[i].data.ptr` 引用着。`queueInLoop` 把析构推迟到事件处理完毕后执行。

### 为什么每个 EventLoop 一个 TimerQueue？
定时器在哪个 loop 创建就在哪个 loop 触发。`addTimer` / `cancel` / `handleRead` 全程单线程，零锁竞争。旧的"全局 TimerQueue + 跨线程 cancel"方案在 handleRead 回调期间修改 timers_ 导致迭代器失效。

### 为什么 HttpContext 放在 TcpConnection 里？
消除 `std::map<TcpConnection*, HttpContext>` 的跨线程竞态。每个连接的 context 读写只在自己所属的 EventLoop 线程中发生。TcpConnection 析构时自动回收，不会 map 残留。

## 已知局限

- HTTP 协议仅支持 GET/POST，不支持 chunked transfer-encoding、URL 解码、pipeline
- 无日志系统（cout/cerr 散落）
- ThreadPool 无背压/队列上限
- 无 SSL/TLS
- 无静态文件服务 / sendfile 零拷贝
- 多 Reactor 是最简单的轮询分发，不感知负载

## 参考资料

- [muduo — 陈硕的 C++ 网络库](https://github.com/chenshuo/muduo)
- [The C10K Problem](http://www.kegel.com/c10k.html)
- Linux man: `epoll(7)`, `timerfd_create(2)`, `eventfd(2)`
