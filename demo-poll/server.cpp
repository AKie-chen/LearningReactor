#include <string>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>

int main() {
    int fd=socket(AF_INET,SOCK_STREAM,0); //创建一个socket，AF_INET表示IPv4协议，SOCK_STREAM表示TCP协议，0表示默认协议

    sockaddr_in addr; //定义一个sockaddr_in结构体，用于存储服务器的地址信息
    addr.sin_family=AF_INET; //设置地址族为IPv4
    addr.sin_port=htons(8080); //设置端口号为8080
    addr.sin_addr.s_addr=INADDR_ANY; //设置IP地址为任意地址

    bind(fd,(sockaddr*)&addr,sizeof(addr)); //将socket绑定到指定的地址和端口上
    listen(fd,5); //监听socket，允许最多5个连接

    struct pollfd fds[1024]; //定义一个pollfd数组，用于存储需要监听的文件描述符
    fds[0].fd = fd; //将服务器socket添加到pollfd数组
    fds[0].events = POLLIN; //设置监听事件为可读事件
    int nfds = 1; //记录当前需要监听的文件描述符数量

    while(true){
        int react=poll(fds,nfds,5000); //等待文件描述符变为可读状态，超时时间为5秒
        if(react>0){//有fd可读
            for(int i=0;i<nfds;i++){
                if(fds[i].revents & (POLLIN | POLLHUP | POLLERR)){ //如果某个文件描述符变为可读状态
                    if(fds[i].fd == fd){ //如果是服务器socket，说明有新的连接请求到来
                        sockaddr_in client_addr;
                        socklen_t client_len = sizeof(client_addr);
                        int client_fd = accept(fd, (sockaddr*)&client_addr, &client_len); //接受一个连接
                        if(client_fd==-1){
                            std::cerr<<"Failed to accept connection"<<std::endl;
                            continue;
                        }
                        fds[nfds].fd = client_fd; //将新的连接socket添加到pollfd数组
                        fds[nfds].events = POLLIN; //设置监听事件为可读事件
                        nfds++; //增加需要监听的文件描述符数量
                        std::cout<<"Accepted a new connection"<<std::endl;

                        fcntl(client_fd, F_SETFL, O_NONBLOCK); //将新的连接socket设置为非阻塞模式
                    }else{ //如果是连接socket，说明有数据可读
                        char buffer[1024];
                        ssize_t bytes_read = read(fds[i].fd, buffer, sizeof(buffer)); //读取数据
                        if(bytes_read > 0){
                            std::cout<<"Received data: "<<std::string(buffer, bytes_read)<<std::endl;
                            send(fds[i].fd, buffer, bytes_read, 0); //将数据原样发送回客户端
                        }else if(bytes_read == 0){
                            std::cout<<"Connection closed by client"<<std::endl;
                            close(fds[i].fd); //关闭连接socket
                            fds[i] = fds[nfds-1]; //将最后一个文件描述符移动到当前位置
                            nfds--; //减少需要监听的文件描述符数量
                            i--; //调整循环索引，以便检查移动后的文件描述符
                        }else{
                            std::cerr<<"Error reading from socket"<<std::endl;
                        }
                    }
                }
            }
        }else if(react==0){
            std::cout<<"Timeout occurred, no data received"<<std::endl;
        }else{
            std::cerr<<"Error occurred in poll"<<std::endl;
        }
    }

    close(fd); //关闭服务器socket
    return 0;
}
