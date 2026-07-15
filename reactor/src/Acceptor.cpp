#include "Acceptor.h"
#include "Log.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

Acceptor::Acceptor(EventLoop* loop, uint16_t port)
            :loop_(loop),
            listenfd_(socket(AF_INET,SOCK_STREAM,0)), 
            channel_(listenfd_,loop_)
{
    ; //创建一个socket，AF_INET表示IPv4协议，SOCK_STREAM表示TCP协议，0表示默认协议
    fcntl(listenfd_, F_SETFL, O_NONBLOCK); //将socket设置为非阻塞模式

    sockaddr_in addr; //定义一个sockaddr_in结构体，用于存储服务器的地址信息
    addr.sin_family=AF_INET; //设置地址族为IPv4
    addr.sin_port=htons(port); //设置端口号为8080
    addr.sin_addr.s_addr=INADDR_ANY; //设置IP地址为任意地址

    int optval = 1;
    setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)); //设置socket选项，允许地址重用
    
    bind(listenfd_,(sockaddr*)&addr,sizeof(addr)); //将socket绑定到指定的地址和端口上
}

Acceptor::~Acceptor(){ close(); }

void Acceptor::close() {
    if (listenfd_ >= 0) {
        ::close(listenfd_);
        listenfd_ = -1;
    }
}

void Acceptor::setNewConnectionCallback(const NewConnectionCallback cb)//设置新连接回调函数
{
    newConnectionCallback_ = cb;
}

int Acceptor::fd()//返回fd
{
    return listenfd_;
}

void Acceptor::listen(int listenNum)//开启监听
{
    ::listen(listenfd_,listenNum); //监听socket，允许最多5个连接
    handleRead();
}

void Acceptor::handleRead()  //处理监听
{
    channel_.setReadCallback([&](){ // 设置可读事件的回调函数
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd;

        while((client_fd = accept(channel_.fd(), (sockaddr*)&client_addr, &client_len)) != -1){//循环接受连接，直到没有连接请求为止
            newConnectionCallback_(client_fd,client_addr);//
        }

        if(errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR << "accept error" << strerror(errno);
        }
    });
    channel_.enableReading(); // 使能可读事件
}