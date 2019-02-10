#ifndef __DB_CONNECTOR_H
#define __DB_CONNECTOR_H

#include <vector>

#include <database/DBConfig.hpp>

/**
*    Database Connector Interface
*
*    Basic functionality is always a key value style db
*    In rdbms this can be extended
**/
class DBConnector {
public:

    /**
    *  DB GET
    *  Key
    **/
    virtual std::vector<std::string> keys(const std::string& pattern) = 0;
    virtual std::string get(const std::string& key) = 0;
    virtual std::string getRange(const std::string& key, unsigned int from, unsigned int to) = 0;
    virtual std::pair<std::string, int> getWithTtl(const std::string& key) = 0;
    virtual bool exists(const std::string& key) = 0;

    /**
    *  DB SET / SETEX
    *  Key
    **/
    virtual bool set(const std::string& key, const std::string& value) = 0;
    virtual bool setEx(const std::string& key, int ttl, const std::string& value) = 0;
    virtual bool expire(const std::string& key, int ttl) = 0;
    
    /**
    *  DB DEL
    *  Key
    **/
    virtual bool del(const std::string& key) = 0;

    /**
    *  DB PING
    **/
    virtual std::string ping() = 0;

    /**
    *  DB TTL
    *  Key
    **/
    virtual int ttl(const std::string& key) = 0;

    /**
    *  DB Can execute SQL Query
    **/
    virtual bool canExecuteSQL() { return false; };
};
#endif // !__DB_CONNECTOR_H
