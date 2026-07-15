# Reactor 网络编程学习仓库

从 `select` 到多线程 Reactor HTTP 服务器，逐步深入 Linux 高性能网络编程。

## 学习路线

```
demo-select        I/O 多路复用入门, fd_set + select()
    ↓
demo-poll          突破 1024 fd 限制, pollfd 数组
    ↓
demo-epoll-LT      内核事件驱动, epoll LT 模式
    ↓
demo-epoll-ET      边缘触发, 非阻塞 I/O, readv 双缓冲
    ↓
reactor/           EventLoop → Channel → Buffer → TcpConnection
                   → Acceptor + TcpServer → HTTP → TimerQueue
                   → ThreadPool → Multi-Reactor
```

## 目录结构

```
Reactor/
├── demo-select/            # 阶段 1: select() 回显服务器
│   ├── server.cpp
│   ├── client.cpp
│   └── CMakeLists.txt
│
├── demo-poll/              # 阶段 2: poll() 回显服务器
│   ├── server.cpp
│   ├── client.cpp
│   └── CMakeLists.txt
│
├── demo-epoll-LT/          # 阶段 3: epoll LT 回显服务器
│   ├── server.cpp
│   ├── client.cpp
│   └── CMakeLists.txt
│
├── demo-epoll-ET/          # 阶段 4: epoll ET 回显服务器
│   ├── server.cpp          #        (Reactor 改造的起点)
│   ├── client.cpp
│   └── CMakeLists.txt
│
└── reactor/                # 阶段 5: 多线程 Reactor HTTP 服务器 (8 步优化)
    ├── include/            # 19 个头文件
    ├── src/                # 17 个源文件
    ├── CMakeLists.txt
    └── README.md           # 详细架构文档
```

## 四个 I/O 模型对比

| 模型 | 实现 | fd 上限 | 通知方式 | 内核开销 |
|------|------|---------|----------|----------|
| `select` | `demo-select` | 1024 | 轮询全部 fd | O(n) |
| `poll` | `demo-poll` | 无上限 | 轮询全部 fd | O(n) |
| `epoll LT` | `demo-epoll-LT` | 无上限 | 只返回就绪 fd | O(1) |
| `epoll ET` | `demo-epoll-ET` | 无上限 | 只通知状态变化 | O(1) 最少 |

### 各阶段关键知识点

**demo-select**: `FD_ZERO` / `FD_SET` / `select()` 的 fd_set 大小写传递、1024 硬限制

**demo-poll**: `pollfd` 结构体、`revents` 字段、事件遍历复杂度

**demo-epoll-LT**: `epoll_create1` / `epoll_ctl` / `epoll_wait`、LT 模式的重复通知

**demo-epoll-ET**: 非阻塞 I/O + `while` 读到 EAGAIN、`readv` + `iovec[2]` 批量读取

**reactor/**: 完整的事件驱动架构，详见 [reactor/README.md](reactor/README.md)

## 编译所有 Demo

```bash
# 逐个编译
cd demo-select && mkdir -p build && cd build && cmake .. && make
cd demo-poll && mkdir -p build && cd build && cmake .. && make
cd demo-epoll-LT && mkdir -p build && cd build && cmake .. && make
cd demo-epoll-ET && mkdir -p build && cd build && cmake .. && make

# 运行 (终端 1)
./demo-epoll-ET/build/server

# 测试 (终端 2)
./demo-epoll-ET/build/client
# 或
nc localhost 8080
```

## Reactor HTTP 服务器

8 步迭代，从单文件 epoll echo 演进到具备生产级基础能力的 HTTP 服务器。

| # | 优化 | 产出 |
|---|------|------|
| 1 | 结构化日志 | LogStream + Logger, 5 级过滤, 时间戳 + 文件行号 |
| 2 | 优雅关闭 | SignalHandler, eventfd + epoll 集成 POSIX 信号 |
| 3 | HTTP 错误处理 | 400/403/404/405/413/500/505, ParseError 分类 |
| 4 | 路由 + 静态文件 | Router (method+path), StaticFileHandler (realpath 防穿越) |
| 5 | 配置系统 | CLI + 配置文件, 两遍扫描 (CLI 优先) |
| 6 | TCP 优化 | TCP_NODELAY, SO_KEEPALIVE, 可配置 backlog, 连接上限 |
| 7 | 指标监控 | 6 个 atomic 计数器, /stats JSON, lock-free |
| 8 | 稳定性修复 | shutdown 完整关闭, pthread TPP 崩溃 workaround |
| 9 | shared_ptr 重构 | use-after-free 修复, 连接生命周期 shared_ptr 管理, 高并发零崩溃 |
| 9 | shared_ptr 重构 | use-after-free 修复, 连接生命周期由 shared_ptr 管理, 压测零崩溃 |

```bash
cd reactor && mkdir -p build && cd build && cmake .. && make
./main

# 命令行参数
./main -p 9090 -i 2 -w 8 -d ./public -t 30 --log-level DEBUG

# 测试
curl http://localhost:8080/                  # 路由 → Hello, World!
curl http://localhost:8080/index.html         # 静态文件
curl http://localhost:8080/stats              # 指标 JSON
wrk -t4 -c100 -d30s http://127.0.0.1:8080/
```

详细架构、性能数据、设计决策见 [reactor/README.md](reactor/README.md)。

## 压测概览

测试环境: AMD Ryzen 7 7735H (4核8线程), Linux 6.6.88, Release 编译, 单机回环

### 并发-吞吐量曲线

| 并发连接 | 10 | 50 | 100 | 300 | 500 | 800 | 1000 | 1500 | 2000 |
|----------|----|----|-----|-----|-----|-----|------|------|------|
| QPS | 10k | 14k | 14.6k | 19.3k | 21.5k | 24.4k | 24.7k | **26.4k** | 25.6k |
| P50 | 0.6ms | 3.2ms | 6.6ms | 14.6ms | 21.7ms | 31.5ms | 38.2ms | 54.0ms | 74.2ms |

### 多场景 Summary

| wrk 场景 | 吞吐量 | P50 | P99 |
|----------|--------|-----|-----|
| 4t × 100 conn × 30s | 13,828 req/s | 6.90 ms | 14.01 ms |
| 4t × 500 conn × 30s | 16,573 req/s | 30.09 ms | 73.69 ms |
| 4t × 1500 conn × 10s (峰值) | **26,376 req/s** | 53.95 ms | 110.54 ms |
| 静态文件 (131B HTML) | 13,663 req/s | 7.05 ms | 14.16 ms |

> 累计 **175 万**请求，**零崩溃**，**零错误**。峰值吞吐受限于 4 核 CPU 饱和。

## 核心技术栈

| 机制 | 用途 |
|------|------|
| `epoll` ET | 事件驱动 I/O |
| `timerfd_create` | 定时器 (CLOCK_MONOTONIC) |
| `eventfd` | 跨线程唤醒 + 信号通知 |
| `readv` / `write` | 分散/聚集 I/O |
| `fcntl(O_NONBLOCK)` | 非阻塞 fd |
| `SO_REUSEADDR` | 地址重用, 快速重启 |
| `TCP_NODELAY` | 禁用 Nagle, 降低延迟 |
| `SO_KEEPALIVE` | 死连接检测 |
| `realpath(3)` | 路径穿越防护 |
| `sigaction` | POSIX 信号处理 |
| `CLOCK_MONOTONIC` | 不受系统时间跳变影响 |
| `MSG_NOSIGNAL` | 避免 SIGPIPE |
| `std::atomic` | 无锁计数器 (Metrics) |

## 环境要求

- Linux kernel ≥ 2.6.27
- CMake ≥ 3.10
- GCC ≥ 8 (C++17)
- pthread

## 参考资料

- [muduo — 陈硕的 C++ 网络库](https://github.com/chenshuo/muduo)
- [The C10K Problem](http://www.kegel.com/c10k.html)
- Beej's Guide to Network Programming
- Linux man: `epoll(7)`, `timerfd_create(2)`, `eventfd(2)`, `readv(2)`
