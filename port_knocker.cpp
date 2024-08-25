#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/bpf.h>

// 由于内核和BPF版本问题，简陋的解决bpf_stats_type报错的方法
enum bpf_stats_type {
    BPF_STATS_RUN_TIME = 0,
};
#include <bpf/bpf.h>

#include <bpf/libbpf.h>
#include <linux/if_link.h>

#define MAP_PATH "/sys/fs/bpf/tc/globals/allowed_clients"

// 用于存储打开的 eBPF 映射文件描述符
int bpf_map_fd = -1;

// 检查客户端是否在允许列表中
bool is_client_allowed(const struct sockaddr_in& client_addr) {
    __be32 client_ip = client_addr.sin_addr.s_addr;
    __u32 value;
    
    if (bpf_map_lookup_elem(bpf_map_fd, &client_ip, &value) == 0) {
        return true;
    }
    return false;
}

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

// 将XDP程序附加到指定的网络接口上
void attach_xdp_program(const char *ifname) {
    struct bpf_object *obj;
    int prog_fd;
    int ifindex;
    
    // 获取网卡接口索引
    ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
        perror("Failed to get ifindex");
        return;
    }

    // 打开编译好的eBPF程序对象文件
    obj = bpf_object__open_file("udp_filter.o", nullptr);
    if (!obj) {
        perror("Failed to open BPF object file");
        return;
    }

    // 加载eBPF程序到内核
    if (bpf_object__load(obj)) {
        perror("Failed to load BPF object");
        bpf_object__close(obj);
        return;
    }

    // 获取名为"udp_filter"的eBPF程序的文件描述符
    prog_fd = bpf_program__fd(bpf_object__find_program_by_name(obj, "udp_filter"));
    if (prog_fd < 0) {
        perror("Failed to find BPF program");
        bpf_object__close(obj);
        return;
    }

    // 将eBPF程序附加到网卡接口
    if (bpf_set_link_xdp_fd(ifindex, prog_fd, 0) < 0) {
        perror("Failed to attach XDP program to interface");
        bpf_object__close(obj);
        return;
    }

    // 固定 eBPF 映射到指定路径
    struct bpf_map *map = bpf_object__find_map_by_name(obj, "allowed_clients");
    if (!map) {
        perror("Failed to find BPF map");
        bpf_object__close(obj);
        return;
    }
    int map_fd = bpf_map__fd(map);
    if (map_fd < 0) {
        perror("Failed to get BPF map fd");
        bpf_object__close(obj);
        return;
    }
    if (bpf_obj_pin(map_fd, MAP_PATH) < 0) {
        perror("Failed to pin BPF map");
        bpf_object__close(obj);
        return;
    }

    std::cout << "成功将XDP程序附加到接口 " << ifname << std::endl;
}

// 初始化XDP程序
void initialize_xdp(const char* interface) {
    if (access(MAP_PATH, F_OK) != -1) {
        if (bpf_obj_get(MAP_PATH) >= 0) {
            unlink(MAP_PATH); // 删除旧的固定点
        }
    }
    attach_xdp_program(interface);
}

// 等待正确的敲门包
bool wait_for_knock(int map_fd) {
    std::cout << "等待正确的敲门包以启动服务器..." << std::endl;
    while (true) {
        __be32 next_key;
        if (bpf_map_get_next_key(map_fd, nullptr, &next_key) == 0) {
            std::cout << "检测到正确的敲门包，准备启动服务器..." << std::endl;
            return true;
        } else {
            std::cout << "未检测到正确的敲门包，继续等待..." << std::endl;
        }
        sleep(1);
    }
}

// TCP服务器函数
void tcp_server(uint16_t port) {
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
        exit(EXIT_FAILURE);
    }

    // 开始监听连接
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "TCP 端口 " << port << " 已开放，仅对已认证的客户端可用" << std::endl;

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_addrlen = sizeof(client_addr);
        new_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_addrlen);
        if (new_socket < 0) {
            perror("accept failed");
            continue;
        }
        
        // 检查连接是否来自允许的客户端
        if (is_client_allowed(client_addr)) {
            std::cout << "允许的客户端连接成功: " << inet_ntoa(client_addr.sin_addr) << std::endl;
            std::thread(handle_client, new_socket).detach();
        } else {
            std::cout << "拒绝非法客户端连接: " << inet_ntoa(client_addr.sin_addr) << std::endl;
            close(new_socket);
        }
    }

    close(server_fd);
}

int main() {
    const char *interface = "lo"; // 替换为你实际使用的网络接口名称
    uint16_t tcp_port = 7897;  // 需要开放的 TCP 端口

    // 初始化XDP程序
    initialize_xdp(interface);

    // 打开 eBPF map
    bpf_map_fd = bpf_obj_get(MAP_PATH);
    if (bpf_map_fd < 0) {
        perror("Failed to open eBPF map");
        exit(EXIT_FAILURE);
    }

    // 等待正确的敲门包
    if (wait_for_knock(bpf_map_fd)) {
        // 启动TCP服务器
        std::cout << "敲门成功，启动TCP服务器..." << std::endl;
        tcp_server(tcp_port);
    }

    close(bpf_map_fd);
    return 0;
}
