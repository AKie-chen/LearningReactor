#include "Log.h"
#include "EventLoop.h"
#include "Channel.h"
#include "Buffer.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "TimerQueue.h"
#include "ThreadPool.h"
#include <signal.h>
#include "SignalHandler.h"

int main() {
    EventLoop loop;
    TcpServer server(&loop, 8080, 4); // 创建TcpServer对象，监听8080端口，使用4个子线程处理连接
    ThreadPool threadPool(4); // 创建线程池，4个工作线程

    SignalHandler signalHandler(&loop);
    signalHandler.addSignal(SIGINT); // 注册 SIGINT 和 SIGTERM 信号处理
    signalHandler.addSignal(SIGTERM);
    signalHandler.setShutdownCallback([&loop, &server]() {
        LOG_INFO << "Shutdown signal received, stopping server...";
        server.shutdown(); // 关闭服务器，释放资源
        loop.quit(); // 停止事件循环
    });

    auto resetTimer = [&](TcpConnection* conn){
            if(conn->timerId() != 0){
                conn->getLoop()->timerQueue().cancel(conn->timerId());
                conn->setTimerId(0);
            }

            //设置新定时器：10秒后forceclose
            timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            int64_t now = ts.tv_sec * 1'000'000 + ts.tv_nsec / 1'000;
            int64_t expiration = now + 10 * 1'000'000;  // 10 秒后
    
            int64_t timerId = conn->getLoop()->timerQueue().addTimer([conn]() {
                conn->forceClose();
            }, expiration);
    
            conn->setTimerId(timerId);
        };
    server.setConnectionCallback([&resetTimer](TcpConnection* conn){
        resetTimer(conn);
        conn->setCloseCallback([](TcpConnection* conn){
            if (conn->timerId() != 0) {// 取消定时器
                conn->getLoop()->timerQueue().cancel(conn->timerId());
                conn->setTimerId(0);
            }
        });
    });
    server.setMessageCallback([&resetTimer,&threadPool](TcpConnection* conn,Buffer* buf){
        HttpContext& ctx = conn->context();
        HttpRequest req;
        if(!ctx.parseRequest(buf, &req)) return;//数据不够等下个包

        // 处理完复位，等待下一个请求
        ctx.reset();
        //重置定时器
        resetTimer(conn);

        threadPool.run([conn]() { // 在工作线程中处理请求
            // 构造响应
            HttpResponse resp;
            resp.setStatusCode(HttpResponse::k200Ok);
            resp.setStatusMessage("OK");
            resp.setBody("Hello, World!\n");

            // 设置必要头部
            resp.addHeader("Content-Length", std::to_string(resp.body().size()));
            resp.addHeader("Connection", "Keep-Alive");

            // 序列化发送
            std::string data = resp.toString();
            // I/O 切回主线程
            conn->getLoop()->queueInLoop([conn, data = std::move(data)]() {
                conn->send(data);
            });
        });
    });
    
    server.start();
    LOG_INFO << "Reactor HTTP server listening on port 8080, 4 IO threads, 4 worker threads";
    loop.loop(); // 启动事件循环，等待和处理事件
    return 0;
}