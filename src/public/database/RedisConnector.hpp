#ifndef __REDISCONNECTOR_H__
#define __REDISCONNECTOR_H__

#include <cpp_redis/core/client.hpp>
#include <cpp_redis/core/types.hpp>
#include <cpp_redis/misc/error.hpp>

#include <database/DBConnector.hpp>
#include <main.hpp>

class RedisConnector : public DBConnector {

private:
    DBConfig config;
    
public:

    RedisConnector(const RedisConnector&) = delete;
    RedisConnector& operator=(const RedisConnector&) = delete;
    RedisConnector(RedisConnector&&) = delete;
    RedisConnector& operator=(RedisConnector&&) = delete;

    RedisConnector(const DBConfig& Config);
    ~RedisConnector();

    std::shared_ptr<cpp_redis::client> client;

    /*
    *  DB GET
    *  Key
    */
    std::vector<std::string> keys(const std::string& prefix);
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
