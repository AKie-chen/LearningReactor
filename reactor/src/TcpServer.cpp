#include "TcpServer.h"
#include "Log.h"
#include "Metrics.h"
#include <fcntl.h>
#include <unistd.h>

TcpServer::TcpServer(EventLoop* loop, uint16_t port, size_t numSubThreads)
    : loop_(loop),
      acceptor_(loop_, port)
{
    for (size_t i = 0; i < numSubThreads; ++i) {
        subLoops_.push_back(std::unique_ptr<EventLoopThread>(new EventLoopThread()));
    }
}

TcpServer::~TcpServer() {}

void TcpServer::setMessageCallback(const MessageCallback cb)
{
    messageCallback_ = cb;
}

void TcpServer::setConnectionCallback(const ConnectionCallback cb)
{
    connectionCallback_ = cb;
}

void TcpServer::start(int listenNum)
{
    acceptor_.setNewConnectionCallback([this](int client_fd, const sockaddr_in& client_addr) {

        if (connectionCount_ >= maxConnections_) {
            ::close(client_fd);
            return;
        }

        connectionCount_++;
        Metrics::instance().activeConnections++;
        EventLoop* ioLoop = subLoops_.empty() ? loop_ : subLoops_[next_++ % subLoops_.size()]->getLoop();

        auto conn = std::make_shared<TcpConnection>(client_fd, ioLoop);
        {
            std::lock_guard<std::mutex> lock(connMutex_);
            connections_.insert(conn);
        }

        // 连接关闭时从集合中移除，释放 server 持有的 shared_ptr
        conn->setOnDestroy([this, raw = conn.get()]() {
            connectionCount_--;
            Metrics::instance().activeConnections--;
            {
                std::lock_guard<std::mutex> lock(connMutex_);
                for (auto it = connections_.begin(); it != connections_.end(); ++it) {
                    if (it->get() == raw) {
                        connections_.erase(it);
                        break;
                    }
                }
            }
        });

        conn->setMessageCallback(messageCallback_);
        conn->setConnectionCallback([this](TcpConnection::ptr c) {
            this->connectionCallback_(c);
        });

        fcntl(client_fd, F_SETFL, O_NONBLOCK);
        ioLoop->queueInLoop([conn]() {
            conn->connectEstablished();
        });
        LOG_DEBUG << "Accepted new connection, fd=" << client_fd;
    });
    acceptor_.listen(listenNum);
}

void TcpServer::shutdown()
{
    acceptor_.close();      // 1. 停止接受新连接
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        connections_.clear();   // 2. 先释放连接（Channel 析构需要 loop_ 存活）
    }
    subLoops_.clear();      // 3. 再停止 IO 线程
}