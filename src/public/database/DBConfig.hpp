#pragma once

#ifndef __DB_CONFIG_HPP__
#define __DB_CONFIG_HPP__

#include <vector>

enum DBType {
    MY_SQL,
    REDIS,
    SQLITE
};

class DBSQLStatementTemplates {
public:
    std::string statementName;
    std::string query;
    unsigned short queryParamsCnt;
};

struct DBConfig {
    std::string connectionName;
    DBType dbType;
    std::string ip;
    unsigned short int port;
    std::string dbname;
    std::string user;
    std::string password;

    std::vector<DBSQLStatementTemplates> statements;
};

#endif