#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main(){
    int fd=socket(AF_INET,SOCK_STREAM,0); //创建一个socket，AF_INET表示IPv4协议，SOCK_STREAM表示TCP协议，0表示默认协议

    sockaddr_in addr; //定义一个sockaddr_in结构体，用于存储服务器的地址信息
    addr.sin_family=AF_INET; //设置地址族为IPv4
    addr.sin_port=htons(8080); //设置端口号为8080
    addr.sin_addr.s_addr=INADDR_ANY; //设置IP地址为任意地址

    if(connect(fd,(sockaddr*)&addr,sizeof(addr))==-1){ //连接服务器
        std::cerr<<"Failed to connect to server"<<std::endl;
        return 1;
    }
    std::cout<<"Connected to server"<<std::endl;

    while(1){
        std::string input;
        std::cout<<"Enter message to send (or 'q' to quit): ";
        std::getline(std::cin, input);
        if(input=="q"){
            break;
        }
        ssize_t bytes_sent = send(fd, input.c_str(), input.size(), 0); //发送数据
        if(bytes_sent == -1){
            std::cerr<<"Failed to send data"<<std::endl;
            break;
        }

        char buffer[1024];
        ssize_t bytes_received = recv(fd, buffer, sizeof(buffer), 0); //接收数据
        if(bytes_received > 0){ 
            std::cout<<"Received from server: "<<std::string(buffer, bytes_received)<<std::endl;
        }else if(bytes_received == 0){
            std::cout<<"Server closed the connection"<<std::endl;
            break;       
        }else{
            std::cerr<<"Failed to receive data"<<std::endl;
            break;
        }
        
    }
    close(fd); //关闭客户端socket
    return 0;
}