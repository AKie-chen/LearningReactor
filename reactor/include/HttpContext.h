#pragma once
#include "Buffer.h"
#include "HttpRequest.h"

class HttpContext{
public:
    enum ParseState { kExpectRequestLine, kExpectHeaders, kExpectBody, KGotCompleteRequest };

    HttpContext();

    //返回true = 完整请求解析到req
    //返回false = 数据不够，等待更多数据（下次EPOLLIN继续）
    bool parseRequest(Buffer* buf, HttpRequest* req);

    void reset();//一个请求处理完，复位等待下一个

private:
    bool parseRequestLine(std::string& line, HttpRequest* req);
    bool parseHeader(std::string& line, HttpRequest* req);
    int findCrlf(Buffer* buf, const char* cl, std::string& line);

    ParseState state_;
    size_t contentLength_;//从Content_Length 头部解析出的body长度
};