#include <database/RedisConnector.hpp>

#include <sstream>


RedisConnector::RedisConnector(const DBConfig& Config) {
    this->config = config;

    
    try {

        this->client = std::make_shared<cpp_redis::client>();
        this->client->connect(config.ip, config.port);

        if (!this->client->is_connected()) {
            throw std::runtime_error("Redis client could not connect");
        }

        if (!config.password.empty()) {
            auto resp = this->client->auth(config.password);
            this->client->sync_commit();
            auto result = resp.get();
            if (result.is_error()) {
                throw std::runtime_error("Redis client could not connect. Wrong credentials");
            }
        }

    }
    catch (cpp_redis::redis_error& e) {
        throw std::runtime_error(std::string("Error creating the redis connector: ") + e.what() );
    }
}

RedisConnector::~RedisConnector() {
    
}

#define EXEC_COMMAND(command, default_return, result_op) try {\
if (!this->client->is_connected()) {\
    throw std::runtime_error("Redis client could not connect");\
}\
auto resp = this->client->command;\
this->client->sync_commit();\
auto result = resp.get();\
if (result.is_error() || result.is_null()) {\
    return default_return;\
}\
return result_op;\
}\
catch (cpp_redis::redis_error& e) {\
    return default_return;\
}

std::string RedisConnector::get(const std::string& key) {
    
    EXEC_COMMAND(get(key), "", result.as_string())

}

std::string RedisConnector::getRange(const std::string& key, unsigned int from, unsigned int to) {
    
    EXEC_COMMAND(getrange(key, from, to), "", result.as_string())

}

std::pair<std::string, int> RedisConnector::getWithTtl(const std::string& key) {
    return { this->get(key), this->ttl(key) };
}

bool RedisConnector::set(const std::string& key, const std::string& value) {
    
    EXEC_COMMAND(set(key, value), false, true)

}

bool RedisConnector::setEx(const std::string& key, int ttl, const std::string& value) {
    
    EXEC_COMMAND(setex(key, ttl, value), false, true)

}

bool RedisConnector::expire(const std::string& key, int ttl) {

    EXEC_COMMAND(expire(key, ttl), false, result.as_integer())

}

bool RedisConnector::exists(const std::string& key) {
    
    EXEC_COMMAND(exists({ key }), false, result.as_integer())

}

bool RedisConnector::del(const std::string& key) {
    
    EXEC_COMMAND(del({ key }), false, result.as_integer())

}

std::string RedisConnector::ping() {
    
    EXEC_COMMAND(ping("true"), "false", result.as_string())

}


int RedisConnector::ttl(const std::string& key) {

    EXEC_COMMAND(ttl(key), -1, std::max((int)result.as_integer(), -1))

}
