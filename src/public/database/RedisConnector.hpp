#ifndef __REDISCONNECTOR_H__
#define __REDISCONNECTOR_H__


#include <database/DBConnector.hpp>

#include <mutex>
#include <cstdarg>

#define PCRE_STATIC 1
#include <pcre.h>

struct redisContext; //forward declare

#define REDISCONNECTOR_MAXCONNECTION_RETRIES 3

class RedisConnector : public DBConnector {

private:
    DBConfig config;

    std::mutex contextMutex;
    redisContext *context;

    pcre * setValueRegex;

    void _reconnect(bool force);

    //EpochlibDBExecute execute(const char *RedisCommand, ...);

public:
    RedisConnector();
    ~RedisConnector();

    bool init(DBConfig Config);

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
    int ttl(const std::string& key);
};

#endif
