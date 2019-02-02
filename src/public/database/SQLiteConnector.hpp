#pragma once

#ifndef __SQLITE_CONNECTOR_H__
#define __SQLITE_CONNECTOR_H__

#include <SQLiteCpp/SQLiteCpp.h>

#include <database/DBConnector.hpp>
#include <main.hpp>

namespace SQLiteConnector_Detail {

    typedef std::shared_ptr< SQLiteDBHolder > db_holder_ref;
    typedef std::shared_ptr< SQLite::Database > db_ref;
    
    class SQLiteDBHolder {
    public:
        std::mutex SQLiteDBMutex;
        db_ref SQLiteDB;
        SQLiteDBHolder() {};
        ~SQLiteDBHolder() {
            // TODO
        }
    };

    static std::map< std::string, db_holder_ref > db_holder_refs = {};
    static std::mutex db_holder_refs_mutex;

};

class SQLiteConnector : public DBConnector {
private:

    DBConfig config;
    std::string defaultKeyValTableName = "KeyValueTable";
    bool extendedLogging = false;

public:

    SQLiteConnector(const DBConfig& config);
    ~SQLiteConnector();

    /**
    *  DB GET
    *  Key
    **/
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