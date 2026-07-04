#include "HttpContext.h"
#include<algorithm>
#include<cstring>

HttpContext::HttpContext():state_(kExpectRequestLine), contentLength_(0){}

int HttpContext::findCrlf(Buffer* buf, const char* cl ,std::string& line)//查找请求行/请求头
{
    const char* begin = buf->peek();
    const char* end = begin + buf->readableBytes();

    auto crlf = std::search(begin, end, cl, cl + strlen(cl));

    if (crlf == end) return -1;  // 没找到完整数据，等更多数据

    line.assign(begin, crlf);         // 不含cl的行内容
    buf->retrieve((crlf - begin) + strlen(cl));     // ← 在这里 retrieve！消费 行内容cl

    return 0;
}

//返回true = 完整请求解析到req
//返回false = 数据不够，等待更多数据（下次EPOLLIN继续）
bool HttpContext::parseRequest(Buffer* buf, HttpRequest* req)
{
    if(state_ == kExpectRequestLine){
        std::string line;
        if(findCrlf(buf,"\r\n", line) < 0) return false;//数据不够

        if (!parseRequestLine(line, req)) return false;  // 格式错误

        state_ = kExpectHeaders;
    }

    if(state_ == kExpectHeaders){
        while(1){
            std::string line;
            if(findCrlf(buf,"\r\n", line) < 0) return false;
            if(line.empty()) break;
            if(!parseHeader(line, req)) return false;
        }

        state_ = (contentLength_ > 0) ? kExpectBody : KGotCompleteRequest;
    }

    if(state_ == kExpectBody){
        if (buf->readableBytes() < contentLength_) return false;  // 数据不够
        req->setBody(buf->retrieve(contentLength_));
        state_ = KGotCompleteRequest;
    }

    return true;
}

void HttpContext::reset()//一个请求处理完，复位等待下一个
{
    state_ = kExpectRequestLine;
    contentLength_ = 0;
}

bool HttpContext::parseRequestLine(std::string& line, HttpRequest* req)
{
    size_t sp1 = line.find(' ', 0);//第一个空格位置
    size_t sp2 = line.find(' ', sp1 + 1);//第二个空格位置

    std::string method = line.substr(0, sp1);//设置方法
    if(method == "GET") req->setMethod(HttpRequest::kGet);
    else if(method == "POST") req->setMethod(HttpRequest::kPost);

    req->setPath(line.substr(sp1 + 1, sp2 - sp1 -1));
    req->setVersion(line.substr(sp2 + 1));
    
    return true;
}

bool HttpContext::parseHeader(std::string& line, HttpRequest* req)
{
    size_t colon = line.find(':');//一行头为Host: localhost,找":"
    if(colon == std::string::npos) return false;

    std::string key = line.substr(0,colon);
    size_t valBegin = colon + 1;

    if (valBegin < line.size() && line[valBegin] == ' ') valBegin++;  // 跳过 ": " 的空格
    std::string value = line.substr(valBegin);

    req->addHeader(key, value);
    if (key == "Content-Length") contentLength_ = std::stoul(value);

    return true;
}