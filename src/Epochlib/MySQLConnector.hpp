#ifndef __MySQLConnector_H__
#define __MySQLConnector_H__

/* Default Epochlib defines */
#include "defines.hpp"

#include "DBConnector.hpp"

#include <mysql/mysql.h>

class MySQLConnector : public DBConnector {
private:
    EpochlibConfigDB config;

    MYSQL * mysql = nullptr;
    MYSQL * setupCon();
    void closeCon(MYSQL * con);
    
    bool __get(const std::string& table, const std::string& _key, std::string& _result);

    bool createTable(MYSQL * con, const std::string& tablename);

public:
    MySQLConnector();
    ~MySQLConnector();


    bool init(EpochlibConfigDB Config);

    /*
    *  DB GET
    *  Key
    */
    std::string get(const std::string& key);
    std::string getRange(const std::string& key, const std::string& value, const std::string& value2);
    std::string getTtl(const std::string& key);
    std::string getbit(const std::string& key, const std::string& value);
    std::string exists(const std::string& key);

    /*
    *  DB SET / SETEX
    */
    std::string set(const std::string& key, const std::string& value);
    std::string setex(const std::string& key, const std::string& ttl, const std::string& value);
    std::string expire(const std::string& key, const std::string& ttl);
    std::string setbit(const std::string& key, const std::string& bitidx, const std::string& value);

    /*
    *  DB DEL
    *  Key
    */
    std::string del(const std::string& key);

    /*
    *  DB PING
    */
    std::string ping();

    /*
    *  DB LPOP with a given prefix
    */
    std::string lpopWithPrefix(const std::string& Prefix, const std::string& Key);

    /*
    *  DB TTL
    *  Key
    */
    std::string ttl(const std::string& Key);

    /*
    *  DB LOG
    */
    std::string log(const std::string& Key, const std::string& Value);

    std::string increaseBancount();
};

#endif
