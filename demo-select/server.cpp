#include <string>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>

int main(){
    int fd=socket(AF_INET,SOCK_STREAM,0); //创建一个socket，AF_INET表示IPv4协议，SOCK_STREAM表示TCP协议，0表示默认协议

    sockaddr_in addr; //定义一个sockaddr_in结构体，用于存储服务器的地址信息
    addr.sin_family=AF_INET; //设置地址族为IPv4
    addr.sin_port=htons(8080); //设置端口号为8080
    addr.sin_addr.s_addr=INADDR_ANY; //设置IP地址为任意地址

    bind(fd,(sockaddr*)&addr,sizeof(addr)); //将socket绑定到指定的地址和端口上
    listen(fd,5); //监听socket，允许最多5个连接

    fd_set readfds;;
    FD_ZERO(&readfds); //初始化文件描述符集合
    FD_SET(fd, &readfds); //将服务器socket添加到文件描述符集合
    struct timeval timeout;
    int max_fd=fd; //记录当前最大的文件描述符

    while(true){
        fd_set tempfds=readfds; //创建一个临时的文件描述符集合，用于select函数
        timeout.tv_sec = 5; //设置超时时间为5秒
         
        int react=select(max_fd+1, &tempfds, nullptr, nullptr, &timeout); //等待文件描述符集合中的socket变为可读状态
        if(react>0){
            if(FD_ISSET(fd,&tempfds)){//如果服务器socket变为可读状态，说明有新的连接请求到来
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(fd, (sockaddr*)&client_addr, &client_len); //接受一个连接
                if(client_fd==-1){
                    std::cerr<<"Failed to accept connection"<<std::endl;
                    continue;
                }
                
                FD_SET(client_fd, &readfds); //将新的连接socket添加到文件描述符集合
                std::cout<<"Accepted a new connection"<<std::endl;

                fcntl(client_fd, F_SETFL, O_NONBLOCK); //将新的连接socket设置为非阻塞模式
                if(client_fd > max_fd){
                    max_fd = client_fd;
                }
            }else{
                for(int i=0;i<=max_fd;i++){
                    if(FD_ISSET(i,&tempfds) && i!=fd){ //如果某个连接socket变为可读状态，说明有数据可读
                        char buffer[1024];
                        ssize_t bytes_read = read(i, buffer, sizeof(buffer)); //读取数据
                        if(bytes_read > 0){
                            std::cout<<"Received data: "<<std::string(buffer, bytes_read)<<std::endl;
                        }else if(bytes_read == 0){
                            std::cout<<"Connection closed by client"<<std::endl;
                            close(i); //关闭连接socket
                            FD_CLR(i, &readfds); //将连接socket从文件描述符集合中移除
                        }else{
                            std::cerr<<"Error reading from socket"<<std::endl;
                        }
                    }
                }
            }
        }else if(react==0){
            std::cout<<"No incoming connections within 5 seconds"<<std::endl;
        }else{
            std::cerr<<"Error in select"<<std::endl;
            break;  
        }
    }
    close(fd); //关闭服务器socket
    return 0;

}