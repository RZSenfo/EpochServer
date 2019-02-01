#ifndef __REDISCONNECTOR_H__
#define __REDISCONNECTOR_H__

#include <database/DBConnector.hpp>
#include <main.hpp>

class RedisConnector : public DBConnector {

private:
    DBConfig config;
    

    
    // place holder
    std::string _DBExecToSQF(const std::string& x, SQF_RETURN_TYPE y);
    std::string execute(const std::string& x);
    std::string execute(const std::string& x, const std::string& y);
    std::string execute(const std::string& x, const std::string& y, const std::string& z);
    std::string execute(const std::string& x, const std::string& y, const std::string& z, const std::string& a);

public:
    RedisConnector(const DBConfig& Config);
    ~RedisConnector();

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
