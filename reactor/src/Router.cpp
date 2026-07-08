#include "Router.h"

// 注册路由：精确匹配 method + path
void Router::addRoute(HttpRequest::Method method, const std::string& path, Handler handler) {
    routes_[{method, path}] = handler;
}

// 查找并执行，返回 true 表示匹配到了
bool Router::route(const HttpRequest& req, HttpResponse* resp) {
    auto it = routes_.find({req.method(), req.path()});
    if (it != routes_.end()) {
        it->second(req, resp);
        return true;
    }
    return false;
}

bool Router::RouteKey::operator<(const RouteKey& other) const {
    if (method != other.method) return method < other.method;
    return path < other.path;
}
