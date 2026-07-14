#pragma once
#include "Channel.h"
#include "Buffer.h"
#include "Timer.h"
#include "HttpContext.h"
#include <string>
#include <functional>

class TcpConnection{
public:
    //回调类型
    using ConnectionCallback = std::function<void(TcpConnection*)>;//连接状态变化
    using MessageCallback = std::function<void(TcpConnection*,Buffer*)>;//消息变化
    using CloseCallback = std::function<void(TcpConnection*)>;//关闭消息回调

    TcpConnection(int fd,EventLoop* loop);
    ~TcpConnection();

    void send(const std::string& data);//发送数据（先缓冲，再写）
    void forceClose();//主动关闭
    void connectEstablished();//启动连接
    void setMessageCallback(const MessageCallback& cb);//连接状态回调
    void setConnectionCallback(const ConnectionCallback& cb);//设置消息回调
    void setCloseCallback(const CloseCallback& cb);//设置关闭消息回调
    void setOnDestroy(std::function<void()> cb) { onDestroy_ = std::move(cb); }
    HttpContext& context() { return context_; }
    int64_t timerId() const { return timerId_; }
    void setTimerId(int64_t id) { timerId_ = id; }

    int fd() const;
    EventLoop* getLoop() const;
private:
    int fd_;                //管理的fd
    EventLoop* loop_;       //所属的loop
    Channel channel_;       //非指针，生命周期跟随示例对象
    Buffer inputBuffer_;    //读缓冲
    Buffer outputBuffer_;   //写缓冲
    HttpContext context_;   //HTTP上下文对象，用于处理HTTP请求和响应
    int64_t timerId_ = 0;   //定时器ID，用于管理连接的超时处理

    void handleRead();      //EPOLLIN回调
    void handleWrite();     //EPOLLOUT回调
    void handleClose();     //关闭/出错时调用
    void destroy();         //销毁对象--queueInLoop

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    CloseCallback closeCallback_;
    std::function<void()> onDestroy_; // server/资源管理回调，在 handleClose 时调用
};