#include "Buffer.h"
#include <unistd.h>
#include <sys/socket.h>
#include <cstring>
#include <sys/uio.h>

Buffer::Buffer():writeIndex_(0),readIndex_(0){}
Buffer::~Buffer(){}

Buffer::ReadResult Buffer::readFd(int fd)// 从文件描述符中读取数据到缓冲区
{   
    while(true){
        char extrabuf[65536]; //额外缓冲区

        iovec iov[2];//离散缓冲区
        iov[0].iov_base = &buf_[writeIndex_];
        iov[0].iov_len = writeableBytes();
        iov[1].iov_base = extrabuf;
        iov[1].iov_len = sizeof(extrabuf);
        
        ssize_t bytes_read = readv(fd, iov, 2); // 从文件描述符中读取数据到缓冲区，返回读取的字节数
        if(bytes_read > 0){
            if(static_cast<size_t>(bytes_read) <= writeableBytes()){
                writeIndex_ += bytes_read;       // 全部数据在 buf_ 内
            }else{
                size_t inBuf = writeableBytes();
                writeIndex_ = buf_.size();       // buf_ 写满
                append(extrabuf, bytes_read - inBuf); // 溢出部分追加到扩容区
            }
            continue;
        }else if(bytes_read == 0){
            return kClosed;//客户端关闭，返回关闭信息
        }else{
            if(errno == EAGAIN || errno == EWOULDBLOCK){ // 如果没有数据可读了，说明已经读取完了
                return kSuccess;//数据读完，返回成功信息
            }else{ // 如果发生了其他错误，说明连接出现了问题，需要关闭连接
                return kError;//发生错误，返回错误信息
            }
        }
    }
}

Buffer::ReadResult Buffer::writeFd(int fd)// 将缓冲区中的数据写入到文件描述符中
{
    size_t remaining = writeIndex_ - readIndex_;
    while(remaining > 0){
        ssize_t n = send(fd, buf_.data() + readIndex_, remaining, MSG_NOSIGNAL);
        if(n > 0){
            readIndex_ += n;
            remaining -= n;        // 处理短写：只推进已发送的字节数
        }else if(errno == EAGAIN || errno == EWOULDBLOCK){
            return kSuccess;                 // socket 发送缓冲区满，等下次 EPOLLOUT 再写
        }else{
            return kError;                // 真正的发送错误
        }
    }

    return kSuccess;
}

std::string Buffer::retrieveAll()// 从缓冲区中读取所有数据，并重置索引
{   
    std::string result(buf_.data() + readIndex_, writeIndex_ - readIndex_);
    readIndex_ = 0;
    writeIndex_ = 0;
    buf_.resize(0);//缓冲区置空

    // 空闲时回收大块内存
    this->shrinkIfLarge();

    return result;
}

std::string Buffer::retrieve(size_t len)// 从缓冲区中读取指定长度的数据，并更新读取索引
{
    if(len > readableBytes()){
        len = readableBytes(); // 如果请求的长度超过可读数据的字节数，则调整为可读数据的字节数
    }
    if(len == 0){
        return "";             // 无数据可读，返回空字符串
    }
    std::string result(buf_.data() + readIndex_, len); // 从缓冲区中读取指定长度的数据
    readIndex_ += len; // 更新读取索引
    if(readIndex_ == writeIndex_){
        readIndex_ = 0;
        writeIndex_ = 0;
    }
    return result;
}

size_t Buffer::readableBytes()// 获取缓冲区中可读数据的字节数
{
    return writeIndex_ - readIndex_;
}

size_t Buffer::writeableBytes()//获取缓冲区可写入数据的字节数
{
    return buf_.size() - writeIndex_;
}

void Buffer::prepend(const char* data, size_t len)// 在缓冲区前面添加数据
{
    buf_.insert(buf_.begin(), data, data + len); // 在缓冲区头部插入数据
    readIndex_ += len; // 更新读取索引
    writeIndex_ += len; // 更新写入索引
}

void Buffer::append(const char* data, size_t len)// 将数据追加到缓冲区中
{   
    if(len > (buf_.size() - writeIndex_)){
        buf_.resize(buf_.size() + len); // 数据大小超过了剩余空间，需要扩容
    }
    char* dest = &buf_[writeIndex_];//获取数据尾部
    memcpy(dest,data,len);
    writeIndex_ += len;
}

const char* Buffer::peek() const//返回可读区起始指针，配合readableBytes实用
{
    return buf_.data() + readIndex_;
}

void Buffer::shrinkIfLarge()// 缩小缓冲区大小，如果缓冲区过大
{
    if (buf_.capacity() > 65536) {   // 超过 64KB 就缩小
        buf_.resize(0);
        buf_.shrink_to_fit();
    } else {
        buf_.resize(0); // 仅清空数据，不缩小容量
    }
}