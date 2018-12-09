# WebServer
单线程web服务器，支持GET、POST、HEAD方法，支持静态文件以及动态内容请求

# Version
v0.1

# Getting Started
进入项目目录编译、运行
```
cd WebServer/
make
```

# Usage
1. 启动
```
./server 8687
```
2. 体验静态文件
使用浏览器访问127.0.0.1:8687/static/girl.jpg

3. 体验动态请求
需要先编译处理动态请求文件
```
cd cgi-bin
make
cd ..
```
浏览器访问127.0.0.1:8687/static/adder.html

