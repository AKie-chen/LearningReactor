#include "TcpConnection.h"
#include "EventLoop.h"
#include "Log.h"
#include "Metrics.h"
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

TcpConnection::TcpConnection(int fd,EventLoop* loop) : fd_(fd),loop_(loop),channel_(fd,loop){
    LOG_DEBUG << "TcpConnection created, fd=" << fd_;
}

TcpConnection::~TcpConnection(){
    LOG_DEBUG << "TcpConnection destroyed, fd=" << fd_;
    ::close(fd_);
}

void TcpConnection::send(const std::string& data)//发送数据（先缓冲，再写）
{
    Metrics::instance().bytesSent += data.size();
    if(outputBuffer_.readableBytes() == 0){//写入缓冲区为空，尝试直接发送
        ssize_t n = ::send(fd_, data.data(), data.size(), MSG_NOSIGNAL);
        if(n > 0){
            if(static_cast<size_t>(n) == data.size()) return;//发完了
            outputBuffer_.append(data.data() + n, data.size() - n);//短写，剩余部分写进buffer
        }else if(errno == EAGAIN || errno == EWOULDBLOCK){ //内核缓冲区满了，先放进outputBuffer_
            outputBuffer_.append(data.data(), data.size());
        }else{//发生错误
            handleClose(); 
            return;
        }
    }else{//outputBuffer_里还有数据，先将data数据追加进outputBuffer_
        outputBuffer_.append(data.data(),data.size());
        return;//等EPOLLOUT触发handleWrite
    }

channel_.enableWriting();//有数据要发，开启监听
}

void TcpConnection::forceClose()//主动关闭
{
    handleClose();
}

void TcpConnection::connectEstablished()//启动连接
{   
    int optval = 1;
    setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)); // 关闭Nagle算法
    setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)); // 心跳机制
    channel_.setReadCallback([this](){handleRead();});
    channel_.setWriteCallback([this](){handleWrite();});
    channel_.enableReading();
    connectionCallback_(this);
}

void TcpConnection::setMessageCallback(const MessageCallback& cb)//设置连接状态回调
{   
    messageCallback_ = cb;
}

void TcpConnection::setConnectionCallback(const ConnectionCallback& cb)//设置消息回调
{
    connectionCallback_ = cb;
}

void TcpConnection::setCloseCallback(const CloseCallback& cb)//设置关闭消息回调
{
    closeCallback_ = cb;
}

int TcpConnection::fd() const //返回fd
{
    return fd_;
}

EventLoop* TcpConnection::getLoop() const //返回事件循环的指针
{
    return loop_;
}

void TcpConnection::handleRead()      //EPOLLIN回调
{
    size_t before = inputBuffer_.readableBytes();
    Buffer::ReadResult result = inputBuffer_.readFd(fd_);//将数据读到缓冲区
    size_t received = inputBuffer_.readableBytes() - before;
    if (received > 0) Metrics::instance().bytesReceived += received;

    if(inputBuffer_.readableBytes() > 0){//处理数据
        messageCallback_(this, &inputBuffer_);
    }

    //判断是否关闭
    if(result == Buffer::kClosed){
        handleClose();
    }else if(result == Buffer::kError){
        handleClose();
    }
}

void TcpConnection::handleWrite()     //EPOLLOUT回调
{
    Buffer::ReadResult result = outputBuffer_.writeFd(fd_);
    if(outputBuffer_.readableBytes() == 0){
        channel_.disableWriting();//发完了，关掉EPOLLOTUT
        outputBuffer_.shrinkIfLarge();//缩小缓冲区
    }
    if(result == Buffer::kError){
        handleClose();
    }
}

void TcpConnection::handleClose()     //关闭/出错时调用
{
    LOG_DEBUG << "Connection closing, fd=" << fd_;
    if (onDestroy_) onDestroy_();
    if(closeCallback_) closeCallback_(this);
    destroy();
}

void TcpConnection::destroy()         //销毁对象--queueInLoop
{
    loop_->queueInLoop([this](){
        delete this;
    });
}