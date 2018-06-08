#ifndef __REDISCONNECTOR_H__
#define __REDISCONNECTOR_H__

/* Default Epochlib defines */
#include "defines.hpp"

#include "DBConnector.hpp"

#include <mutex>
#include <cstdarg>

#define PCRE_STATIC 1
#include <pcre.h>

struct redisContext; //forward declare

#define REDISCONNECTOR_MAXCONNECTION_RETRIES 3

class RedisConnector : public DBConnector {
private:
    EpochlibConfigDB config;

    std::mutex contextMutex;
    redisContext *context;

    pcre * setValueRegex;

    void _reconnect(bool force);

    EpochlibDBExecute execute(const char *RedisCommand, ...);

public:
    RedisConnector();
    ~RedisConnector();

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
