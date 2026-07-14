#pragma once
#include "Acceptor.h"
#include "TcpConnection.h"
#include "EventLoopThread.h"
#include <functional>
#include <atomic>

class TcpServer{
public:
    using MessageCallback = std::function<void(TcpConnection*,Buffer*)>;
    using ConnectionCallback = std::function<void(TcpConnection*)>;

    TcpServer(EventLoop* loop,uint16_t port,size_t numSubThreads = 0);
    ~TcpServer();

    void setMessageCallback(const MessageCallback cb);//处理连接的回调函数
    void setConnectionCallback(const ConnectionCallback cb);//连接建立创建定时器的回调函数
    void start(int listenNum);//启动acceptor
    void shutdown(); // 关闭服务器，释放资源
    void setMaxConnections(size_t max) { maxConnections_ = max; }
private:
    EventLoop* loop_;//事件循环
    Acceptor acceptor_;//服务端类
    MessageCallback messageCallback_;//消息回调函数
    ConnectionCallback connectionCallback_;
    std::vector<std::unique_ptr<EventLoopThread>> subLoops_;//线程池
    std::atomic<size_t> connectionCount_{0};//连接计数器
    size_t maxConnections_ = 10000;//最大连接数
    int next_ = 0;//轮询计数器
};
