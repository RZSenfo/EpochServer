#ifndef __MySQLConnector_H__
#define __MySQLConnector_H__

#include <mariadb++/connection.hpp>

#include <database/DBConnector.hpp>
#include <main.hpp>

// flag to use mysql_library_init()
// this is not done by mariadbpp, thus it would not be threadsafe to creation and use connections
namespace MySQLConnector_Detail {
    static bool is_first_connection = true;
};

class MySQLConnector : public DBConnector {
private:
    
    DBConfig config;
    std::shared_ptr<mariadb::connection> con;
    std::string defaultKeyValTableName = "KeyValueTable";
    bool extendedLogging = false;
    
    std::vector<DBSQLStatementTemplate> preparedStatements;

    bool __createKeyValueTable(const std::string& tablename);

public:


    MySQLConnector(const DBConfig& config);
    ~MySQLConnector();

    /*
    *  DB GET
    *  Key
    */
    std::string get(const std::string& key);
    std::string getRange(const std::string& key, unsigned int from, unsigned int to);
    std::pair<std::string, int> getWithTtl(const std::string& key);
    bool exists(const std::string& key);

    /*
    *  DB SET / SETEX
    */
    bool set(const std::string& key, const std::string& value);
    bool setEx(const std::string& key, int ttl, const std::string& value);
    bool expire(const std::string& key, int ttl);
    
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
    int ttl(const std::string& key);
};

#endif
