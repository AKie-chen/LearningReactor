#pragma once
#include "Channel.h"
#include "Buffer.h"
#include "Timer.h"
#include "HttpContext.h"
#include <string>
#include <functional>
#include <memory>
#include <atomic>

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    // 回调类型 — 全部使用 shared_ptr 防止 use-after-free
    using ptr = std::shared_ptr<TcpConnection>;
    using ConnectionCallback = std::function<void(ptr)>;
    using MessageCallback = std::function<void(ptr, Buffer*)>;
    using CloseCallback = std::function<void(ptr)>;

    TcpConnection(int fd, EventLoop* loop);
    ~TcpConnection();

    void send(const std::string& data);
    void forceClose();
    void connectEstablished();
    void setMessageCallback(const MessageCallback& cb);
    void setConnectionCallback(const ConnectionCallback& cb);
    void setCloseCallback(const CloseCallback& cb);
    void setOnDestroy(std::function<void()> cb) { onDestroy_ = std::move(cb); }
    HttpContext& context() { return context_; }
    int64_t timerId() const { return timerId_; }
    void setTimerId(int64_t id) { timerId_ = id; }

    int fd() const;
    EventLoop* getLoop() const;
private:
    int fd_;
    EventLoop* loop_;
    Channel channel_;
    Buffer inputBuffer_;
    Buffer outputBuffer_;
    HttpContext context_;
    int64_t timerId_ = 0;
    std::atomic<bool> closed_{false};   // 防止 handleClose() 重入

    void handleRead();
    void handleWrite();
    void handleClose();
    void destroy();  // 仅移除 epoll 监听，不 delete this

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    CloseCallback closeCallback_;
    std::function<void()> onDestroy_;
};