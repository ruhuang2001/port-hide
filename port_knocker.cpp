#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>


// 处理客户端连接的函数
void handle_client(int client_socket) {
    char buffer[1024] = {0};
    // 从客户端读取数据
    read(client_socket, buffer, 1024);
    std::cout << "收到数据: " << buffer << std::endl;
    
    // 向客户端发送响应
    const char* message = "Hello, client!";
    send(client_socket, message, strlen(message), 0);
    
    // 关闭客户端socket
    close(client_socket);
}

// TCP服务器函数
void tcp_server(uint16_t port, sockaddr_in client_addr) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // 创建socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 设置socket选项，允许地址重用
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // 绑定socket到指定端口
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 开始监听连接
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    std::cout << "TCP 端口 " << port << " 已开放，仅对指定客户端可用" << std::endl;

    while (true) {
        // 接受新的连接
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }
        
        // 检查连接是否来自允许的客户端
        if (address.sin_addr.s_addr == client_addr.sin_addr.s_addr) {
            std::cout << "客户端连接成功" << std::endl;
            // 为新的客户端连接创建一个新线程
            std::thread(handle_client, new_socket).detach();
        } else {
            std::cout << "拒绝非法客户端连接" << std::endl;
            close(new_socket);
        }
    }

    close(server_fd);
}

// UDP监听器函数
void udp_listener(uint16_t udp_port, uint16_t tcp_port, const std::string& secret_knock) {
    int sockfd;
    char buffer[1024];
    struct sockaddr_in servaddr, cliaddr;

    // 创建UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(udp_port);

    // 绑定socket到指定端口
    if (bind(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    socklen_t len;
    int n;
    len = sizeof(cliaddr);

    while (true) {
        // 接收UDP数据
        n = recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr*)&cliaddr, &len);
        std::cout << "收到的敲门数据: " << buffer << std::endl;
        if (n < 0) {
            perror("recvfrom failed");
            continue; // 发生错误时跳过本次循环，继续监听
        }
        buffer[n] = '\0';
        std::string received_knock(buffer);

        // 检查是否收到正确的敲门序列
        if (received_knock == secret_knock) {
            std::cout << "收到合法敲门包，开放TCP端口 " << tcp_port << " 给客户端" << std::endl;
            // 为新的TCP服务器创建一个新线程
            std::thread(tcp_server, tcp_port, cliaddr).detach();
        } else {
            std::cout << "收到非法敲门包，忽略。" << std::endl;
        }
    }
}

int main() {
    uint16_t udp_port = 12345; // 监听的 UDP 端口
    uint16_t tcp_port = 7897;  // 需要开放的 TCP 端口
    std::string secret_knock = "secret"; // 敲门包的内容

    // 启动UDP监听器线程
    std::thread(udp_listener, udp_port, tcp_port, secret_knock).detach();

    // 主线程保持运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::minutes(1));
    }

    return 0;
}
