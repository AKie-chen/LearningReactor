#pragma once
#include <string>

struct ServerConfig{
    // 网络
    uint16_t port = 8080;
    std::string bindAddr = "0.0.0.0";

    // 线程
    size_t ioThreads = 4;
    size_t workerThreads = 4;

    // 静态文件
    std::string staticDir = "./static";
    size_t maxFileSizeMB = 10;

    // 超时
    int64_t connectionTimeoutSec = 10;

    // 日志
    std::string logLevel = "INFO";

};

class ConfigParser {
public:
    // 返回 false 表示需要打印 help 并退出
    bool parse(int argc, char* argv[], ServerConfig& cfg);
    
    // 从文件加载（key=value 格式）
    static bool loadFromFile(const std::string& filepath, ServerConfig& cfg);

private:
    static void printHelp(const char* program);
};