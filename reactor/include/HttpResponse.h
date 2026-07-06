#pragma once
#include <string>
#include <map>
#include "Buffer.h"

class HttpResponse{
public:
    enum HttpStatusCode{ k200Ok = 200, k400BadRequest = 400,
                        k404NotFound = 404, k405MethodNotAllowed = 405,
                        k500InternalServerError = 500,k505HttpVersionNotSupported = 505, };// http状态码枚举

    HttpResponse();// 构造函数
    ~HttpResponse();// 析构函数

    void setStatusCode(HttpStatusCode code);// 设置状态码
    void setStatusMessage(const std::string& msg);// 设置状态消息
    void setBody(const std::string& body);// 设置主体内容
    void addHeader(const std::string& key, const std::string& value);// 添加头部
    void setCloseConnection(bool on);// 设置关闭连接标识
    bool closeConnection() const;// 获取是否关闭连接
    const std::string& body() const;//获取body数据
    const HttpStatusCode& code() const;//获取状态码
    const std::string& msg() const;//获取状态信息
    const std::string& header_value(const std::string& key) const;//获取header对应value 
    static HttpResponse makeError(HttpStatusCode code, const std::string& message);// 创建错误响应对象

    void appendToBuffer(Buffer* buf) const;// 序列化成 HTTP 响应报文
    std::string toString() const;// 将响应对象转换为字符串形式
private:
    HttpStatusCode statusCode_;// 状态码
    std::string statusMessage_;// 状态消息
    std::string body_;// 主体内容
	std::map<std::string, std::string> headers_;
	bool closeConnection_;
};