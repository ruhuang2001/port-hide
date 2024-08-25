#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/udp.h>
#include <stdbool.h>
#include <bpf/bpf_helpers.h>

#define SECRET_KNOCK "secret" // 定义一个秘密敲门的字符串，用于认证
#define MAX_FAILURES 3        // 定义最大失败次数

// 定义一个哈希表，用于存储允许的客户端 IP 地址
struct {
    __uint(type, BPF_MAP_TYPE_HASH);   // 定义为哈希类型的 BPF map
    __uint(max_entries, 1024);         // 最大支持 1024 个条目
    __type(key, __be32);               // 键是 IP 地址（使用网络字节序）
    __type(value, __u32);              // 值是敲门事件计数，设为 1 表示允许
} allowed_clients SEC(".maps");

// 定义一个哈希表，用于存储拒绝的客户端 IP 地址
struct {
    __uint(type, BPF_MAP_TYPE_HASH);   // 定义为哈希类型的 BPF map
    __uint(max_entries, 1024);         // 最大支持 1024 个条目
    __type(key, __be32);               // 键是 IP 地址（使用网络字节序）
    __type(value, __u32);              // 值是失败次数
} denied_clients SEC(".maps");

// 自定义的 ntohs 函数，用于将 16 位数值从网络字节序转换为主机字节序
static __u16 my_ntohs(__u16 val) {
    return (val << 8) | (val >> 8);
}

SEC("xdp")
int udp_filter(struct xdp_md *ctx) {
    // 定义数据包的开始和结束指针
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    
    // 检查以太网头部
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
        return XDP_PASS;    // 如果包长度小于以太网头部长度，直接通过

    // 检查 IP 头部
    struct iphdr *ip = (void *)(eth + 1);
    if ((void *)(ip + 1) > data_end)
        return XDP_PASS;    // 如果包长度小于 IP 头部长度，直接通过

    // 检查是否在拒绝列表中
    __u32 *is_denied = bpf_map_lookup_elem(&denied_clients, &ip->saddr);
    if (is_denied && *is_denied > MAX_FAILURES) {
        return XDP_DROP;  // 如果在拒绝列表中，直接丢弃数据包
    }

    // 检查协议
    if (ip->protocol != IPPROTO_UDP)
        return XDP_PASS;    // 如果不是 UDP 协议，直接通过

    // 检查 UDP 头部
    struct udphdr *udp = (void *)(ip + 1);
    if ((void *)(udp + 1) > data_end)
        return XDP_PASS;   // 如果 UDP 数据包不完整，直接通过

    // 检查 UDP 目标端口
    if (udp->dest != my_ntohs(12345)) {
        return XDP_PASS;  // 敲门包目标端口不是 12345，通过
    } 
    
    // 检查 UDP 载荷数据的起始位置
    char *payload = (char *)(udp + 1);
    int payload_len = data_end - (void *)payload;
    if (payload_len != sizeof(SECRET_KNOCK) - 1)
        return XDP_PASS;  // 如果载荷长度不匹配，直接通过

    // 手动比较载荷内容和 SECRET_KNOCK 定义的密钥
    bool match = true;
    for (int i = 0; i < sizeof(SECRET_KNOCK) - 1; i++) {
        // 在每次访问前检查是否越界
        if ((void *)(payload + i + 1) > data_end || payload[i] != SECRET_KNOCK[i]) {
            match = false;
            break;
        }
    }

    // 匹配后更新允许哈希表
    if (match) {
        __u32 value = 1;
        bpf_map_update_elem(&allowed_clients, &ip->saddr, &value, BPF_ANY);
        return XDP_PASS;
    } else {
        // 失败更新拒绝哈希表
         __u32 *fail_count = bpf_map_lookup_elem(&denied_clients, &ip->saddr);
        if (fail_count) {
            *fail_count += 1;
        } else {
            __u32 initial_fail_count = 1;
            bpf_map_update_elem(&denied_clients, &ip->saddr, &initial_fail_count, BPF_ANY);
        }
    }

    return XDP_PASS;

}

char _license[] SEC("license") = "GPL";
