#ifndef __DB_CONNECTOR_H
#define __DB_CONNECTOR_H

#include <vector>
#include <intercept.hpp>

#include <database/DBConfig.hpp>

/*
    Database Connector Interface

    Basic functionality is always a key value style db
    In rdbms this can be extended
*/
class DBConnector {
public:

    /*
     * DB INIT
     * has to setup a database connection with given config or return false on failure
     * also has to prepare anything for the key value transactions (if thats needed)
     */
    virtual bool init(DBConfig config) = 0;

    /*
    *  DB GET
    *  Key
    */
    virtual std::string get(const std::string& key) = 0;
    virtual std::string getRange(const std::string& key, const std::string& from, const std::string& to) = 0;
    virtual std::string getTtl(const std::string& key) = 0;
    virtual std::string getBit(const std::string& key, const std::string& value) = 0;
    virtual bool exists(const std::string& key) = 0;

    /*
    *  DB SET / SETEX
    *  Key
    */
    virtual bool set(const std::string& key, const std::string& value) = 0;
    virtual bool setEx(const std::string& key, const std::string& ttl, const std::string& value) = 0;
    virtual bool expire(const std::string& key, const std::string& ttl) = 0;
    virtual bool setBit(const std::string& key, const std::string& bitidx, const std::string& value) = 0;

    /*
    *  DB DEL
    *  Key
    */
    virtual bool del(const std::string& key) = 0;

    /*
    *  DB PING
    */
    virtual std::string ping() = 0;

    /*
    *  DB TTL
    *  Key
    */
    virtual int ttl(const std::string& key) = 0;

    /*
    *  DB Can execute SQL Query
    */
    virtual bool canExecuteSQL() { return false; };
};
#endif // !__DB_CONNECTOR_H
