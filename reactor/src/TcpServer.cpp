#include "TcpServer.h"
#include "Log.h"
#include <fcntl.h>

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

void TcpServer::start()
{
    acceptor_.setNewConnectionCallback([this](int client_fd,const sockaddr_in& client_addr){

        EventLoop* ioLoop = subLoops_.empty() ? loop_ : subLoops_[next_++ % subLoops_.size()]->getLoop();
        TcpConnection* conn = new TcpConnection(client_fd, ioLoop);

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
    acceptor_.listen();
}

void TcpServer::shutdown() // 关闭服务器，释放资源
{
    subLoops_.clear(); // 清空子线程池，触发 EventLoopThread 的析构函数，停止子线程
}