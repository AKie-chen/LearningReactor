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
└── reactor/                # 阶段 5: 多线程 Reactor HTTP 服务器
    ├── include/            # 13 个头文件
    ├── src/                # 15 个源文件
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

```bash
cd reactor && mkdir -p build && cd build && cmake .. && make
./main

# 测试
curl -v http://localhost:8080/
wrk -t4 -c100 -d30s http://127.0.0.1:8080/
```

详细架构、性能数据、设计决策见 [reactor/README.md](reactor/README.md)。

## 核心技术栈

| 机制 | 用途 |
|------|------|
| `epoll` ET | 事件驱动 I/O |
| `timerfd_create` | 定时器 (CLOCK_MONOTONIC) |
| `eventfd` | 跨线程唤醒 |
| `readv` / `write` | 分散/聚集 I/O |
| `fcntl(O_NONBLOCK)` | 非阻塞 fd |
| `SO_REUSEPORT` | 多进程负载均衡 |
| `CLOCK_MONOTONIC` | 不受系统时间跳变影响 |
| `MSG_NOSIGNAL` | 避免 SIGPIPE |

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
