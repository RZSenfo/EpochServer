#pragma once

#ifndef __DB_CONFIG_HPP__
#define __DB_CONFIG_HPP__

#include <vector>

enum DBType {
    MY_SQL,
    REDIS,
    SQLITE
};

enum DBSQLStatementParamType {
    NUMBER,
    BOOL,
    STRING,
    ARRAY
};

struct DBSQLStatementTemplate {
    std::string statementName;
    std::string query;
    std::vector<DBSQLStatementParamType> params;
    std::vector<DBSQLStatementParamType> result;
    bool isInsert;
};

struct DBConfig {
    std::string connectionName;
    DBType dbType;
    std::string ip;
    unsigned short int port;
    std::string dbname;
    std::string user;
    std::string password;

    std::vector<DBSQLStatementTemplate> statements;
};

#endif