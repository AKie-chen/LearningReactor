#include "HttpResponse.h"

void HttpResponse::setStatusCode(HttpStatusCode code)// 设置状态码
{
    statusCode_ = code;
}

void HttpResponse::setStatusMessage(const std::string& msg)// 设置状态消息
{
    statusMessage_ = msg;
}

void HttpResponse::setBody(const std::string& body)// 设置主体内容
{
    body_ = body;
}

void HttpResponse::addHeader(const std::string& key, const std::string& value)// 添加头部
{
    headers_[key] = value;
}

void HttpResponse::setCloseConnection(bool on)// 设置关闭连接标识
{
    closeConnection_ = on;
}

bool HttpResponse::closeConnection() const// 获取是否关闭连接
{
    return closeConnection_;
}

const std::string& HttpResponse::body() const//获取body数据
{
    return body_;
}

const HttpResponse::HttpStatusCode& HttpResponse::code() const//获取状态码
{
    return statusCode_;
}

const std::string& HttpResponse::msg() const//获取状态信息
{
    return statusMessage_;
}

const std::string& HttpResponse::header_value(const std::string& key) const//获取header对应value 
{
    auto it = headers_.find(key);
    static const std::string empty;
    return it != headers_.end() ? it->second : empty;
}

void HttpResponse::appendToBuffer(Buffer* buf) const// 序列化成 HTTP 响应报文
{   
    //序列化行
    std::string statusLine = "HTTP/1.1 " + std::to_string(statusCode_) + " " +statusMessage_ + "\r\n";
    buf->append(statusLine.data(), statusLine.size());

    //序列化头
    for(auto& [key, value] : headers_){
        std::string h = key + ": " + value + "\r\n";
        buf->append(h.data(), h.size());
    }

    //空行
    buf->append("\r\n", 2);
    
    //序列化Body
    buf->append(body_.data(), body_.size());
}

std::string HttpResponse::toString() const// 将响应对象转换为字符串形式
{
    std::string result;
    // 预估大小，减少 realloc
    size_t estimate = 64 + statusMessage_.size() + body_.size();
    for (auto& [k, v] : headers_) estimate += k.size() + v.size() + 4;
    result.reserve(estimate);

    // 状态行
    result += "HTTP/1.1 " + std::to_string(statusCode_) + " " + statusMessage_ + "\r\n";
    // 头部
    for (auto& [k, v] : headers_) {
        result += k + ": " + v + "\r\n";
    }
    // 空行
    result += "\r\n";
    // Body
    result += body_;

    return result;
}