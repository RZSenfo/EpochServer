/* Default Epochlib defines */
#include "defines.hpp"
#include <mutex>
#include <cstdarg>

struct redisContext;

#ifndef __REDISCONNECTOR_H__
#define __REDISCONNECTOR_H__

#define REDISCONNECTOR_MAXCONNECTION_RETRIES 3

class RedisConnector {
private:
	EpochlibConfigRedis config;

	std::mutex contextMutex;
	redisContext *context;

	void _reconnect(bool force);

public:
	RedisConnector(EpochlibConfigRedis Config);
	~RedisConnector();

	EpochlibRedisExecute execute(const char *RedisCommand, ...);
};

#endif
