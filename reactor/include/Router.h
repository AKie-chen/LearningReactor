#pragma once
#include <functional>
#include <unordered_map>
#include "HttpRequest.h"
#include "HttpResponse.h"

class Router {
public:
    using Handler = std::function<void(const HttpRequest&, HttpResponse*)>;

    Router() = default;
    ~Router() = default;

    // 注册路由：精确匹配 method + path
    void addRoute(HttpRequest::Method method, const std::string& path, Handler handler);

    // 查找并执行，返回 true 表示匹配到了
    bool route(const HttpRequest& req, HttpResponse* resp);

private:
    struct RouteKey { // 存储 method 和 path
        HttpRequest::Method method;
        std::string path;
        bool operator<(const RouteKey& other) const;  // 用于 map key
    };
    std::map<RouteKey, Handler> routes_;  // 或 std::map<RouteKey, Handler>
};
