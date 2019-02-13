#pragma once

#ifndef __SQLITE_CONNECTOR_H__
#define __SQLITE_CONNECTOR_H__

#include <SQLiteCpp/SQLiteCpp.h>

#include <database/DBConnector.hpp>
#include <main.hpp>

namespace SQLiteCon_Detail {

    
    typedef std::shared_ptr< SQLite::Database > DbRef;
    class SQLiteDBHolder {
    public:
        std::mutex SQLiteDBMutex;
        DbRef SQLiteDB;
        SQLiteDBHolder() {};
        ~SQLiteDBHolder() {
            // TODO check if cleanup is needed
            // i.e. db close, save/commit
        }
    };
    typedef std::shared_ptr< SQLiteDBHolder > DbHolderRef;

    static std::map< std::string, DbHolderRef > dbHolderRefs = {};
    static std::mutex dbHolderRefsMutex;

};

class SQLiteConnector : public DBConnector {
private:

    DBConfig config;
    std::string defaultKeyValTableName = "KeyValueTable";
    bool extendedLogging = false;

    SQLiteCon_Detail::DbHolderRef holderRef = nullptr;

public:

    SQLiteConnector(const SQLiteConnector&) = delete;
    SQLiteConnector& operator=(const SQLiteConnector&) = delete;
    SQLiteConnector(SQLiteConnector&&) = delete;
    SQLiteConnector& operator=(SQLiteConnector&&) = delete;

    SQLiteConnector(const DBConfig& config);
    ~SQLiteConnector();

    /**
    *  DB GET
    *  Key
    **/
    std::vector<std::string> keys(const std::string& prefix);
    std::string get(const std::string& key);
    std::string getRange(const std::string& key, unsigned int from, unsigned int to);
    std::pair<std::string, int> getWithTtl(const std::string& key);
    bool exists(const std::string& key);

    /**
    *  DB SET / SETEX
    *  Key
    **/
    bool set(const std::string& key, const std::string& value);
    bool setEx(const std::string& key, int ttl, const std::string& value);
    bool expire(const std::string& key, int ttl);

    /**
    *  DB DEL
    *  Key
    **/
    bool del(const std::string& key);

    /**
    *  DB PING
    **/
    std::string ping();

    /**
    *  DB TTL
    *  Key
    **/
    int ttl(const std::string& key);

    /**
    *  DB Can execute SQL Query
    **/
    bool canExecuteSQL() { return false; };
};

#endif