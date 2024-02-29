#ifndef MYSQLCONNPOOL_H
#define MYSQLCONNPOOL_H

#include <queue>
#include <memory>
#include "MySQLConn.h"
#include "../locker/Locker.h"
using namespace std;

class MySQLConnPool {
public:
    static MySQLConnPool* getMySQLConnPool();
    shared_ptr<MySQLConn> getMySQLConn();
    ~MySQLConnPool();
private:
    MySQLConnPool();
    bool parseJsonFile();
    void produceConnection();
    void recycleConnection();
    void addConnection();

    string m_ip;
    string m_user;
    string m_passwd;
    string m_dbName;
    unsigned short m_port;
    int m_minSize;
    int m_maxSize;
    int m_timeout;
    int m_maxIdleTime;
    queue<MySQLConn*> m_connectionQ;
    locker m_queuelocker;
    cond m_queueProduceCond;
    sem m_queueSem;
};

#endif