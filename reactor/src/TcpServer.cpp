#include "TcpServer.h"
#include "Log.h"
#include "Metrics.h"
#include <fcntl.h>
#include <unistd.h>

TcpServer::TcpServer(EventLoop* loop,uint16_t port,size_t numSubThreads)
    :loop_(loop),
    acceptor_(loop_,port)
{
    for(size_t i = 0; i < numSubThreads; ++i)
    {
        subLoops_.push_back(std::unique_ptr<EventLoopThread>(new EventLoopThread()));
    }
}

TcpServer::~TcpServer(){}

void TcpServer::setMessageCallback(const MessageCallback cb)
{
    messageCallback_ = cb;
}

void TcpServer::setConnectionCallback(const ConnectionCallback cb)//连接建立创建定时器的回调函数
{
    connectionCallback_ = cb;
}

void TcpServer::start(int listenNum)
{
    acceptor_.setNewConnectionCallback([this](int client_fd,const sockaddr_in& client_addr){

        if(connectionCount_ >= maxConnections_) { //超过最大连接数，关闭连接
            ::close(client_fd);
            return;
        }

        connectionCount_++; //连接数加一
        Metrics::instance().activeConnections++; //活跃连接数加一
        EventLoop* ioLoop = subLoops_.empty() ? loop_ : subLoops_[next_++ % subLoops_.size()]->getLoop();
        TcpConnection* conn = new TcpConnection(client_fd, ioLoop);

        conn->setOnDestroy([this]() { 
            connectionCount_--; // 连接数减一
            Metrics::instance().activeConnections--; // 活跃连接数减一
        });

        conn->setMessageCallback(messageCallback_);
        conn->setConnectionCallback([this](TcpConnection* conn){
            this->connectionCallback_(conn);
            //destory会自动delete
        });
        
        fcntl(client_fd, F_SETFL, O_NONBLOCK); //将新的连接socket设置为非阻塞模式
        ioLoop->queueInLoop([conn](){
            conn->connectEstablished();
        });
        LOG_DEBUG << "Accepted new connection, fd=" << client_fd;
    });
    acceptor_.listen(listenNum);
}

void TcpServer::shutdown() // 关闭服务器，释放资源
{
    acceptor_.close();   // 1. 停止接受新连接
    subLoops_.clear();   // 2. 停止 IO 线程（EventLoop::quit + join），处理完队列中的连接销毁
}