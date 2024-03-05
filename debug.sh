#!/bin/bash

# 编译命令
g++ WebServerMain.cpp ./timer/MinHeapTimer.cpp ./http/HttpConn.cpp ./mysql/MySQLConn.cpp ./mysql/MySQLConnPool.cpp -lpthread -lhiredis -L/usr/lib64 -ljsoncpp -L/usr/lib64/mysql -lmysqlclient -Iinclude -I/usr/include/jsoncpp -std=c++11 -g -o WebServer 

# 检查上一个命令是否成功执行
if [ $? -eq 0 ]; then
    echo "编译成功，可以运行 ./WebServer 来启动服务器"
else
    echo "编译失败，请检查错误信息"
fi
