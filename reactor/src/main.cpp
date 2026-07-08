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
#include "Router.h"
#include "StaticFileHandler.h"

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

    Router router; // 创建路由器对象
    StaticFileHandler staticHandler("./static"); // 创建静态文件处理器对象，指定静态文件目录

    router.addRoute(HttpRequest::kGet, "/", [](const HttpRequest& req, HttpResponse* resp) { // 注册路由
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setBody("Hello, World!");
        resp->addHeader("Content-Length", std::to_string(resp->body().size()));
        resp->addHeader("Content-Type", "text/plain");
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
    server.setMessageCallback([&resetTimer,&threadPool,&router,&staticHandler](TcpConnection* conn,Buffer* buf){
        HttpContext& ctx = conn->context();
        HttpRequest req;

        if(!ctx.parseRequest(buf, &req)) {
            if(ctx.error() == HttpContext::kNoError) return; // 数据不够，继续等待
            else if(ctx.error() == HttpContext::kBadRequest) { // 请求格式错误，直接返回400错误
                conn->getLoop()->queueInLoop([conn]() { 
                    conn->send(HttpResponse::makeError(HttpResponse::k400BadRequest, "Bad Request").toString());
                });
                conn->forceClose(); // 关闭连接
                return;
            }else if(ctx.error() == HttpContext::kMethodNotSupported) { // 方法不支持，返回405错误
                conn->getLoop()->queueInLoop([conn]() {
                    conn->send(HttpResponse::makeError(HttpResponse::k405MethodNotAllowed, "Method Not Supported").toString());
                });
                ctx.reset();
                return; 
            }else if(ctx.error() == HttpContext::kVersionNotSupported) { // HTTP版本不支持，返回505错误
                conn->getLoop()->queueInLoop([conn]() {
                    conn->send(HttpResponse::makeError(HttpResponse::k505HttpVersionNotSupported, "Http Version Not Supported").toString());
                });
                ctx.reset();
                return; 
            }
        }

        // 处理完复位，等待下一个请求
        ctx.reset();
        //重置定时器
        resetTimer(conn);

        threadPool.run([conn,req, &router, &staticHandler]() { // 在工作线程中处理请求
            HttpResponse resp;
            
            if (!router.route(req, &resp)) {
                // 没有精确匹配，尝试静态文件
                if (!staticHandler.handle(req, &resp)) {
                    // 静态文件也处理不了 → 404
                    resp = HttpResponse::makeError(HttpResponse::k404NotFound, "Not Found");
                }
            }

            std::string data = resp.toString();
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