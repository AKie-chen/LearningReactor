#include <string>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <vector>
#include <fcntl.h>
#include <cstring>

int main() {
    int listenfd=socket(AF_INET,SOCK_STREAM,0); //创建一个socket，AF_INET表示IPv4协议，SOCK_STREAM表示TCP协议，0表示默认协议
    fcntl(listenfd, F_SETFL, O_NONBLOCK); //将socket设置为非阻塞模式

    sockaddr_in addr; //定义一个sockaddr_in结构体，用于存储服务器的地址信息
    addr.sin_family=AF_INET; //设置地址族为IPv4
    addr.sin_port=htons(8080); //设置端口号为8080
    addr.sin_addr.s_addr=INADDR_ANY; //设置IP地址为任意地址

    bind(listenfd,(sockaddr*)&addr,sizeof(addr)); //将socket绑定到指定的地址和端口上
    listen(listenfd,5); //监听socket，允许最多5个连接

    int epfd=epoll_create1(0);//创建一个epoll实例，参数0表示默认选项

    struct epoll_event ev;//创建监听fd的事件结构体
    ev.events=EPOLLIN | EPOLLET;//设置事件为可读和边缘触发
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
                        int client_fd;

                        while((client_fd = accept(listenfd, (sockaddr*)&client_addr, &client_len)) != -1){//循环接受连接，直到没有连接请求为止
                            epoll_event client_ev;//创建连接socket的事件结构体
                            client_ev.events = EPOLLIN | EPOLLET; //设置事件为可读和边缘触发
                            client_ev.data.fd = client_fd; //存储连接socket的fd信息
                            epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev);//将连接socket加入epoll中
                            std::cout<<"Accepted a new connection"<<std::endl;

                            fcntl(client_fd, F_SETFL, O_NONBLOCK); //将新的连接socket设置为非阻塞模式
                        }
                    }else{ //如果是连接socket，说明有数据可读
                        char buffer[1024];
                        int total=0; //记录总共读取的字节数
                        while(true){ //循环读取数据，直到没有数据可读
                            char temp_buffer[1024]; //临时缓冲区，用于存储每次读取的数据
                            ssize_t bytes_read = read(events[i].data.fd, temp_buffer, sizeof(temp_buffer)); //读取数据到temp_buffer中，返回读取的字节数
                            
                            if(bytes_read > 0){
                                memcpy(buffer + total, temp_buffer, bytes_read); //将读取到的数据复制到buffer中，注意这里没有处理数据超过1024字节的情况，实际应用中需要考虑这种情况
                                total += bytes_read;
                                continue; //继续读取数据，直到没有数据可读了

                            }else if(bytes_read == 0){//如果读取到0字节，说明客户端关闭了连接
                                std::cout<<"Connection closed by client"<<std::endl;
                                close(events[i].data.fd); //关闭连接socket
                                epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,nullptr); //将连接socket从epoll中移除
                                break; //退出循环，继续等待其他事件

                            }else{  //如果读取到负数，说明发生了错误
                                if(errno == EAGAIN || errno == EWOULDBLOCK){ //如果错误是因为没有数据可读了，说明已经读取完了所有数据   
                                    break;//读空了数据，正常退出
                                }else{  //其他错误，说明发生了异常
                                    std::cerr<<"Error occurred while reading from socket"<<std::endl;
                                    close(events[i].data.fd); //关闭连接socket
                                    epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,nullptr); //将连接socket从epoll中移除
                                    break; //退出循环，继续等待其他事件
                                }
                            }
                        }
                        if(total > 0){ //如果读取到了数据，处理数据并发送响应
                            std::string received_data(buffer, total); //将读取到的数据转换为字符串
                            std::cout<<"Received data: "<<received_data<<std::endl;

                            std::string response = "Echo: " + received_data; //构造响应数据，这里简单地将收到的数据加上"Echo: "前缀作为响应
                            ssize_t bytes_sent = send(events[i].data.fd, response.c_str(), response.size(), 0); //发送响应数据
                            if(bytes_sent == -1){
                                std::cerr<<"Failed to send response"<<std::endl;
                                close(events[i].data.fd); //关闭连接socket
                                epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,nullptr); //将连接socket从epoll中移除
                            }
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
