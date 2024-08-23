# port-hide
“端口隐藏”实现demo

## 如何使用

打开服务器终端
```
git clone 
cd port-hide
g++ -std=c++11 port_knocker.cpp -o port_knocker -pthread
./port_knocker
```

另外开一个终端
```
echo -n "secret" | nc -u localhost 12345
nc localhost 7897

# 然后输入你想传入的内容回车
# 就能在上一个终端看到数据接收成功的状态
```

## 开发过程

### v1.0

考虑到对网络编程不是特别熟悉，所以第一版先能跑通最重要：）

根据端口隐藏的概念，设计了一个简单的程序，实现大致流程如下：
1. 启动UDP监听器，等待敲门数据包。
2. 当接收到正确的敲门数据包后，启动TCP服务器并开放指定端口。
3. 仅允许发送了正确敲门包的客户端连接到TCP服务器。
4. 在TCP连接建立后，处理客户端请求并返回响应信息。

效果图
![v1效果图](./img/runVersion1.png)

## 参考学习资料

[YouTube@Eric O Meehan -- Creating a Web Server from Scratch in C](https://www.youtube.com/watch?v=gk6NL1pZi1M)

[YouTube@Nicholas Day -- C++ Network Programming Part 1: Sockets ](https://www.youtube.com/watch?v=gntyAFoZp-E)

[Wiki: Port knocking](https://en.wikipedia.org/wiki/Port_knocking)

[Jack Huang: C++高性能网络编程](https://huangwang.github.io/2019/10/26/CPlusPlus%E9%AB%98%E6%80%A7%E8%83%BD%E7%BD%91%E7%BB%9C%E7%BC%96%E7%A8%8B/)
