#ifndef MYSQLCONN_H
#define MYSQLCONN_H

#include <string>
#include <mysql/mysql.h>
#include <chrono>
using namespace std;
using namespace chrono;

class MySQLConn {
public:
    /* 初始化数据库连接 */
    MySQLConn();
    /* 释放数据库连接 */
    ~MySQLConn();
    /* 仅线程池使用 */
    bool connect(string& user, string& passwd, string& dbName, string& ip, 
                 unsigned short port = 3306);
    /* insert update delete */
    bool update(string& sql);
    /* select */
    bool query(string& sql);
    /* 遍历查询得到的结果集 */
    bool next();
    /* 得到结果集中的字段值 */
    string getValue(int index);
    /* 错误信息 */
    string getError();
    /* 事务操作 */
    bool transaction();
    /* 事务提交 */
    bool commit();
    /* 事务回滚 */
    bool rollback();

private:
    void freeResult();
    MYSQL* m_conn;
    MYSQL_RES* m_result;
    MYSQL_ROW m_row;
    steady_clock::time_point m_alivetime;
};

#endif