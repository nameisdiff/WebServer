#include "MySQLConn.h"
#include <iostream>

MySQLConn::MySQLConn() : m_conn(NULL), m_result(NULL),
                         m_row(NULL) {
    m_conn = mysql_init(NULL);
}

MySQLConn::~MySQLConn() {
    freeResult();
    if (m_conn != NULL) {
        mysql_close(m_conn);
    }
}

bool MySQLConn::connect(string& user, string& passwd, string& dbName, string& ip,
                        unsigned short port) {
    MYSQL* ptr = mysql_real_connect(m_conn, ip.c_str(), user.c_str(), passwd.c_str(), 
                                    dbName.c_str(), port, NULL, 0);
    if (ptr) {
        return true;
    } else {
        std:cerr << "connect failed:" << mysql_error(m_conn) << std::endl;
        return false;
    }
}

bool MySQLConn::update(string& sql) {
    if (mysql_query(m_conn, sql.c_str())) {
        return false;
    }
    return true;
}

bool MySQLConn::query(string& sql) {
    freeResult();
    if (mysql_query(m_conn, sql.c_str())) {
        return false;
    }
    m_result = mysql_store_result(m_conn);
    return true;
}

bool MySQLConn::next() {
    if (m_result != NULL) {
        m_row = mysql_fetch_row(m_result);
        if (m_row != NULL) {
            return true;
        }
    }
    return false;
}

string MySQLConn::getValue(int index) {
    int rowCount = mysql_num_fields(m_result);
    if (index >= rowCount || index < 0) {
        return string();
    }

    char* val = m_row[index];
    /* 字符串中可能有\0 */
    unsigned long length = mysql_fetch_lengths(m_result)[index];
    return string(val, length);
}

bool MySQLConn::transaction() {
    return mysql_autocommit(m_conn, false);
}

bool MySQLConn::commit() {
    return mysql_commit(m_conn);
}

bool MySQLConn::rollback() {
    return mysql_rollback(m_conn);
}

void MySQLConn::freeResult() {
    if (m_result) {
        mysql_free_result(m_result);
        m_result = NULL;
    }
}

string MySQLConn::getError() {
    string error = string(mysql_error(m_conn));
    return error;
}