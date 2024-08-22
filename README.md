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
echo -n "wrong" | nc -u localhost 12345
nc localhost 7897

# 然后输入你想传入的内容回车
# 就能在上一个终端看到数据接收成功的状态
```

## 开发过程

## 参考资料

https://en.wikipedia.org/wiki/Port_knocking

https://www.cnblogs.com/milton/p/14121214.html