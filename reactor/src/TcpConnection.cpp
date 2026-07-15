#include "TcpConnection.h"
#include "EventLoop.h"
#include "Log.h"
#include "Metrics.h"
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

TcpConnection::TcpConnection(int fd, EventLoop* loop)
    : fd_(fd), loop_(loop), channel_(fd, loop)
{
    LOG_DEBUG << "TcpConnection created, fd=" << fd_;
}

TcpConnection::~TcpConnection()
{
    // fd 由 Channel 析构负责关闭，这里不重复 close
    LOG_DEBUG << "TcpConnection destroyed, fd=" << fd_;
}

void TcpConnection::send(const std::string& data)
{
    Metrics::instance().bytesSent += data.size();
    if (outputBuffer_.readableBytes() == 0) {
        ssize_t n = ::send(fd_, data.data(), data.size(), MSG_NOSIGNAL);
        if (n > 0) {
            if (static_cast<size_t>(n) == data.size()) return;
            outputBuffer_.append(data.data() + n, data.size() - n);
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            outputBuffer_.append(data.data(), data.size());
        } else {
            handleClose();
            return;
        }
    } else {
        outputBuffer_.append(data.data(), data.size());
        return;
    }
    channel_.enableWriting();
}

void TcpConnection::forceClose()
{
    handleClose();
}

void TcpConnection::connectEstablished()
{
    int optval = 1;
    setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
    setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));

    // Channel 回调使用 raw this — 安全，因为 Channel 是 TcpConnection 的成员，
    // disableAll() 后不会再触发回调，且 TcpConnection 由 shared_ptr 保证生命周期
    channel_.setReadCallback([this]() { handleRead(); });
    channel_.setWriteCallback([this]() { handleWrite(); });
    channel_.setErrorCallback([this]() { handleClose(); });
    channel_.enableReading();
    connectionCallback_(shared_from_this());
}

void TcpConnection::setMessageCallback(const MessageCallback& cb)
{
    messageCallback_ = cb;
}

void TcpConnection::setConnectionCallback(const ConnectionCallback& cb)
{
    connectionCallback_ = cb;
}

void TcpConnection::setCloseCallback(const CloseCallback& cb)
{
    closeCallback_ = cb;
}

int TcpConnection::fd() const
{
    return fd_;
}

EventLoop* TcpConnection::getLoop() const
{
    return loop_;
}

void TcpConnection::handleRead()
{
    size_t before = inputBuffer_.readableBytes();
    Buffer::ReadResult result = inputBuffer_.readFd(fd_);
    size_t received = inputBuffer_.readableBytes() - before;
    if (received > 0) Metrics::instance().bytesReceived += received;

    if (inputBuffer_.readableBytes() > 0) {
        messageCallback_(shared_from_this(), &inputBuffer_);
    }

    if (result == Buffer::kClosed) {
        handleClose();
    } else if (result == Buffer::kError) {
        handleClose();
    }
}

void TcpConnection::handleWrite()
{
    Buffer::ReadResult result = outputBuffer_.writeFd(fd_);
    if (outputBuffer_.readableBytes() == 0) {
        channel_.disableWriting();
        outputBuffer_.shrinkIfLarge();
    }
    if (result == Buffer::kError) {
        handleClose();
    }
}

void TcpConnection::handleClose()
{
    // 防止重入：messageCallback_ 中调了 forceClose()，
    // handleRead/handleWrite 后续又会再次调用 handleClose()
    if (closed_.exchange(true)) return;

    // 守卫：保持 shared_ptr 直到函数结束，防止 closeCallback_ 中释放
    // 最后一个引用导致 this 被 delete，后续 destroy() 访问 UAF
    ptr guard = shared_from_this();

    LOG_DEBUG << "Connection closing, fd=" << fd_;
    if (onDestroy_) onDestroy_();
    if (closeCallback_) closeCallback_(guard);
    destroy();
}

void TcpConnection::destroy()
{
    // 从 epoll 移除，不再监听任何事件
    channel_.disableAll();

    // 延迟释放守卫：epoll_wait 返回的 events 数组可能还持有本 Channel 的指针，
    // 如果此时析构 TcpConnection，后续遍历 events 时 Channel 指针会悬空。
    // 将 shared_ptr 放入 pending queue，确保析构发生在 epoll for 循环之后。
    loop_->queueInLoop([guard = shared_from_this()]() {
        // guard 在此销毁，TcpConnection 生命周期安全结束
    });
}