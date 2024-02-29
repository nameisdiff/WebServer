#include "MySQLConnPool.h"
#include <json/json.h>
#include <fstream>
#include <thread>
#include <unistd.h>
#include <iostream>
#include <stdio.h>
using namespace Json;

/* 懒汉单例模式 */
MySQLConnPool* MySQLConnPool::getMySQLConnPool() {
    static MySQLConnPool pool;
    return &pool;
}

bool MySQLConnPool::parseJsonFile() {
    ifstream ifs("./mysql/dbconf.json");
    CharReaderBuilder builder;
    Value root;
    std::string errs;
    
    if (parseFromStream(builder, ifs, &root, &errs)) {
        m_ip = root["ip"].asString();
        m_port = root["port"].asInt();
        m_user = root["username"].asString();
        m_passwd = root["password"].asString();
        m_dbName = root["dbName"].asString();
        m_minSize = root["minSize"].asInt();
        m_maxSize = root["maxSize"].asInt();
        m_maxIdleTime = root["maxIdleTime"].asInt();
        m_timeout = root["timeout"].asInt();
        return true;
    } else {
        std::cerr << "parse json fail :"<< errs << std::endl;
        return false;
    }
}

void MySQLConnPool::addConnection() {
    MySQLConn* conn = new MySQLConn;
    if (!conn->connect(m_user, m_passwd, m_dbName, m_ip, m_port)) {
        printf("connect failed\n");
    }

    m_queuelocker.lock();
    m_connectionQ.push(conn);
    m_queuelocker.unlock();
    m_queueSem.post();
}

MySQLConnPool::MySQLConnPool() {
    /* 加载配置文件 */
    if (!parseJsonFile()) {
        printf("parse Json File failed\n");
        return;
    }

    for (int i = 0; i < m_minSize; ++i) {
        addConnection();
    }
    thread producer(&MySQLConnPool::produceConnection, this);
    thread recycler(&MySQLConnPool::recycleConnection, this);
    producer.detach();
    recycler.detach();
}

void MySQLConnPool::produceConnection() {
    while (true) {
        while (m_connectionQ.size() >= m_minSize) {
            m_queueProduceCond.wait();
        }
        addConnection();
    }
}

void MySQLConnPool::recycleConnection() {
    while (true) {
        sleep(5);
        m_queuelocker.lock();
        while (m_connectionQ.size() > m_minSize) {
            MySQLConn* conn = m_connectionQ.front();
            m_connectionQ.pop();
            delete conn;
        }
        m_queuelocker.unlock();
    }
}

shared_ptr<MySQLConn> MySQLConnPool::getMySQLConn() {
    m_queueSem.wait();
    m_queuelocker.lock();
    shared_ptr<MySQLConn> connptr(m_connectionQ.front(), [this](MySQLConn* conn) {
        m_queuelocker.lock();
        m_connectionQ.push(conn);
        m_queuelocker.unlock();
    });
    m_connectionQ.pop();
    m_queuelocker.unlock();
    m_queueProduceCond.signal();
    return connptr;
}

MySQLConnPool::~MySQLConnPool() {
    while (!m_connectionQ.empty()) {
        MySQLConn* conn = m_connectionQ.front();
        m_connectionQ.pop();
        delete conn;
    }
}