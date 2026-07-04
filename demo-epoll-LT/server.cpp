#include <string>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <vector>
#include <fcntl.h>

int main() {
    int listenfd=socket(AF_INET,SOCK_STREAM,0); //创建一个socket，AF_INET表示IPv4协议，SOCK_STREAM表示TCP协议，0表示默认协议

    sockaddr_in addr; //定义一个sockaddr_in结构体，用于存储服务器的地址信息
    addr.sin_family=AF_INET; //设置地址族为IPv4
    addr.sin_port=htons(8080); //设置端口号为8080
    addr.sin_addr.s_addr=INADDR_ANY; //设置IP地址为任意地址

    bind(listenfd,(sockaddr*)&addr,sizeof(addr)); //将socket绑定到指定的地址和端口上
    listen(listenfd,5); //监听socket，允许最多5个连接

    int epfd=epoll_create1(0);//创建一个epoll实例，参数0表示默认选项

    struct epoll_event ev;//创建监听fd的事件结构体
    ev.events=EPOLLIN;//设置事件为可读
    ev.data.fd=listenfd;//存储fd信息
    epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&ev);//将监听fd加入epoll中

    std::vector<epoll_event> events(1024);//返回就绪事件数组

    while(true){
        int react=epoll_wait(epfd,events.data(),events.size(),5000);//等待事件发生，参数分别为epoll实例、就绪事件数组、最大事件数和超时时间（毫秒）
        
        if(react>0){//有fd可读
            for(int i=0;i<react;i++){
                if(events[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR)){ //如果某个文件描述符变为可读状态
                    if(events[i].data.fd == listenfd){ //如果是服务器socket，说明有新的连接请求到来
                        sockaddr_in client_addr;
                        socklen_t client_len = sizeof(client_addr);
                        int client_fd = accept(listenfd, (sockaddr*)&client_addr, &client_len); //接受一个连接
                        if(client_fd==-1){
                            std::cerr<<"Failed to accept connection"<<std::endl;
                            continue;
                        }
                        epoll_event client_ev;//创建连接socket的事件结构体
                        client_ev.events = EPOLLIN; //设置事件为可读
                        client_ev.data.fd = client_fd; //存储连接socket的fd信息
                        epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev);//将连接socket加入epoll中
                        std::cout<<"Accepted a new connection"<<std::endl;

                        fcntl(client_fd, F_SETFL, O_NONBLOCK); //将新的连接socket设置为非阻塞模式
                    }else{ //如果是连接socket，说明有数据可读
                        char buffer[1024];
                        ssize_t bytes_read = read(events[i].data.fd, buffer, sizeof(buffer)); //读取数据
                        if(bytes_read > 0){
                            std::cout<<"Received data: "<<std::string(buffer, bytes_read)<<std::endl;
                            send(events[i].data.fd, buffer, bytes_read, 0); //将数据原样发送回客户端
                        }else if(bytes_read == 0){
                            std::cout<<"Connection closed by client"<<std::endl;
                            close(events[i].data.fd); //关闭连接socket
                            epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,nullptr); //将连接socket从epoll中移除
                        }else{
                            std::cerr<<"Error reading from socket"<<std::endl;
                            close(events[i].data.fd); //关闭连接socket
                            epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,nullptr); //将连接socket从epoll中移除
                        }
                    }
                }
            }
        }else if(react==0){
            std::cout<<"Timeout occurred, no data received"<<std::endl;
        }else{
            std::cerr<<"Error occurred in epoll"<<std::endl;
        }

    }

    close(listenfd); //关闭服务器socket
    return 0;
}
