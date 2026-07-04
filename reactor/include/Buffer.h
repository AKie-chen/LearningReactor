#pragma once
#include <vector>
#include <string>

class Buffer {
private:
    std::vector<char> buf_; // 存储数据的缓冲区
    size_t readIndex_; // 读取数据的索引
    size_t writeIndex_; // 写入数据的索引

public:
    enum ReadResult { kSuccess, kError, kClosed }; // 读取结果的枚举类型
    
    Buffer();
    ~Buffer();

    ReadResult readFd(int fd);// 从文件描述符中读取数据到缓冲区
    ReadResult writeFd(int fd);// 将缓冲区中的数据写入到文件描述符中
    std::string retrieveAll();// 从缓冲区中读取所有数据，并重置索引
    std::string retrieve(size_t len);// 从缓冲区中读取指定长度的数据，并更新读取索引
    size_t readableBytes();// 获取缓冲区中可读数据的字节数
    size_t writeableBytes();//获取缓冲区可写入数据的字节数
    void prepend(const char* data, size_t len);// 在缓冲区前面添加数据
    void append(const char* data, size_t len);// 将数据追加到缓冲区中
    const char* peek() const;//返回可读区起始指针，配合readableBytes实用
    void shrinkIfLarge();// 缩小缓冲区大小，如果缓冲区过大
};