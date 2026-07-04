#pragma once
#include <iostream>
#include <string>
#include <map>

class HttpRequest{
public:
    enum Method { kInvalid, kGet, kPost, kHead };//设置http方法，空，get，post，获取头

    void setMethod(Method m);//设置方法
    Method method() const;//获取方法

    void setPath(const std::string& path);//设置url
    const std::string& path() const;//获取url
    
    void setVersion(const std::string& ver);//设置http版本，1/1.1
    const std::string& version() const;//获取版本

    void addHeader(const std::string& key, const std::string& value);//添加头
    std::string getHeader(const std::string& key) const;//获取头
    const std::map<std::string, std::string>& headers() const;//头部哈希

    void setBody(const std::string& body);//设置数据
    const std::string& body() const;//获取数据

private:   
    Method method_ = kInvalid;//http方法
    std::string path_;//url路径
    std::string version_;//http版本
    std::map<std::string, std::string> headers_;//头部数据
    std::string body_;//数据
};