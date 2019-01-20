#ifndef __MySQLConnector_H__
#define __MySQLConnector_H__

#include <mariadb++/connection.hpp>

#include <database/DBConnector.hpp>
#include <main.hpp>

class MySQLConnector : public DBConnector {
private:
    
    std::unique_ptr<mariadb::connection> con;
    
    std::vector<DBSQLStatementTemplates> preparedStatements;

    bool __get(const std::string& table, const std::string& _key, std::string& _result);
    bool __createKeyValueTable(const std::string& tablename);

public:
    MySQLConnector();
    ~MySQLConnector();


    bool init(DBConfig config);

    /*
    *  DB GET
    *  Key
    */
    std::string get(const std::string& key);
    std::string getRange(const std::string& key, const std::string& from, const std::string& to);
    std::string getTtl(const std::string& key);
    std::string getbit(const std::string& key, const std::string& value);
    bool exists(const std::string& key);

    /*
    *  DB SET / SETEX
    */
    bool set(const std::string& key, const std::string& value);
    bool setex(const std::string& key, const std::string& ttl, const std::string& value);
    bool expire(const std::string& key, const std::string& ttl);
    bool setbit(const std::string& key, const std::string& bitidx, const std::string& value);

    /*
    *  DB DEL
    *  Key
    */
    bool del(const std::string& key);

    /*
    *  DB PING
    */
    std::string ping();

    /*
    *  DB TTL
    *  Key
    */
    int ttl(const std::string& Key);
};

#endif
