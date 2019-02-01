#include <database/RedisConnector.hpp>

#include <main.hpp>
#include <sstream>


RedisConnector::RedisConnector(const DBConfig& Config) {
    this->config = config;

    this->_reconnect(false);
}

RedisConnector::~RedisConnector() {
    
}

/*
std::string RedisConnector::execute(const char *format, ...) {
    this->_reconnect(0);

    va_list ap;
    EpochlibDBExecute returnObj;
    redisReply *reply = NULL;

    while (reply == NULL) {
        // Lock, execute, unlock
        this->contextMutex.lock();
        va_start(ap, format);
        reply = (redisReply *)redisvCommand(this->context, format, ap);
        va_end(ap);
        this->contextMutex.unlock();

        if (reply->type == REDIS_REPLY_ERROR) {
            returnObj.success = false;
            returnObj.message = reply->str;
            INFO("[Redis] Error command " + std::string(reply->str));
        }
        else {

            if (reply->type == REDIS_REPLY_STRING) {
                returnObj.success = reply->str != NULL;
                returnObj.message = reply->str;
            }
            else if (reply->type == REDIS_REPLY_INTEGER) {
                returnObj.success = true;
                std::stringstream IntToString;
                IntToString << reply->integer;
                returnObj.message = IntToString.str();
            }
        }
    }

    freeReplyObject(reply);

    return returnObj;
}
*/

void RedisConnector::_reconnect(bool _force) {
    // Security context lock
    /*
    this->contextMutex.lock();

    if (this->context == NULL || _force) {
        int retries = 0;
        struct timeval timeout { 1, 50000 };

        do {
            if (this->context != NULL) {
                redisFree(this->context);
            }

            this->context = redisConnectWithTimeout(this->config.ip.c_str(), this->config.port, timeout);

            if (this->context->err) {
                INFO("[Redis] " + std::string(this->context->errstr));
            }

            retries++;
        } while (this->context == NULL || (this->context->err & (REDIS_ERR_IO || REDIS_ERR_EOF) && retries < REDISCONNECTOR_MAXCONNECTION_RETRIES));

        // Too many retries -> exit server with log
        if (retries == REDISCONNECTOR_MAXCONNECTION_RETRIES) {
            INFO("[Redis] Server not reachable");
            exit(1);
        }

        // Password given -> AUTH
        if (!this->config.password.empty()) {
            redisReply *authReply = NULL;

            while (authReply == NULL) {
                authReply = (redisReply *)redisCommand(this->context, "AUTH %s", this->config.password.c_str());
            }
            if (authReply->type == REDIS_REPLY_STRING) {
                if (strcmp(authReply->str, "OK") == 0) {
                    INFO("[Redis] Could not authenticate: " + std::string(authReply->str));
                }
            }

            freeReplyObject(authReply);
        }

        // Database index given -> change database
        int dbIndex = 1;//std::stoi(this->config.dbIndex);
        if (dbIndex > 0) {
            redisReply *selectReply = NULL;

            while (selectReply == NULL) {
                selectReply = (redisReply *)redisCommand(this->context, "SELECT %d", dbIndex);
            }
            if (selectReply->type == REDIS_REPLY_STRING) {
                if (strcmp(selectReply->str, "OK") == 0) {
                    INFO("[Redis] Could not change database: " + std::string(selectReply->str));
                }
            }

            freeReplyObject(selectReply);
        }
    }

    // Unlock
    this->contextMutex.unlock();
    */
}

std::string RedisConnector::getRange(const std::string& _key, unsigned int from, unsigned int to) {
    return this->execute("GETRANGE %s %s %s", _key.c_str(), std::to_string(from).c_str(), std::to_string(to).c_str());
    /*
    if (value.success == 1) {
        
        SQF returnSqf;
        returnSqf.push_number(SQF_RETURN_STATUS::SUCCESS);
        if (value.message.empty()) {
            returnSqf.push_empty_str();
        } else if(value.message[0] == '[') {
            returnSqf.push_array(value.message);
        }
        else {
            returnSqf.push_str(value.message);
        }
        return returnSqf.toArray();
    }
    else {
        return SQF::RET_FAIL();
    }
    */
}

std::string RedisConnector::get(const std::string& _key) {


    auto temp = this->execute("GET %s", _key.c_str());
    return temp;

    /*
    // GET success proceed
    if (temp.success == 1) {
        
        SQF returnSqf;
        returnSqf.push_number(SQF_RETURN_STATUS::SUCCESS); // single row

        if (temp.message.empty()) {
            returnSqf.push_empty_str();
        } else if (temp.message[0] == '[') {
            returnSqf.push_array(temp.message.c_str());
        }
        else {
            returnSqf.push_str(temp.message.c_str(), 0);
        }

        return returnSqf.toArray();
    }
    else {
        return SQF::RET_FAIL();
    }
    */

}

int RedisConnector::getWithTtl(const std::string& _key) {

    // No temp GET found -> GET new one
    auto temp = this->execute("GET %s", _key.c_str());
    return std::stoi(temp);
    /*
    if (temp.success == 1) {

        SQF returnSqf;
        EpochlibDBExecute ttl = this->execute("TTL %s", _key.c_str());

        size_t messageSize = 0;

        returnSqf.push_number(SQF_RETURN_STATUS::SUCCESS); // single row


        if (ttl.success == 1) {
            returnSqf.push_number(std::atol(ttl.message.c_str()));
        }
        else {
            returnSqf.push_number(-1);
        }
        
        if (!temp.message.empty() && temp.message[0] == '[') {
            returnSqf.push_array(temp.message.c_str());
        }
        else {
            returnSqf.push_str(temp.message.c_str(), 0);
        }

        return returnSqf.toArray();
    }
    else {
        return SQF::RET_FAIL();
    }
    */
    
}

bool RedisConnector::set(const std::string& _key, const std::string& _value) {
    // _value not used atm    
    return this->_DBExecToSQF(this->execute("SET %s %s", _key.c_str(), _value.c_str()), SQF_RETURN_TYPE::NOTHING) != "";
    
}

bool RedisConnector::setEx(const std::string& _key, int _ttl, const std::string& _value) {
    return this->_DBExecToSQF(this->execute("SETEX %s %s %s", _key.c_str(), std::to_string(_ttl).c_str(), _value.c_str()), SQF_RETURN_TYPE::NOTHING) != "";
}

bool RedisConnector::expire(const std::string& _key, int _ttl) {
    return this->_DBExecToSQF(this->execute("EXPIRE %s %s", _key.c_str(), std::to_string(_ttl).c_str()), SQF_RETURN_TYPE::NOTHING) != "";
}

bool RedisConnector::exists(const std::string& _key) {
    return this->_DBExecToSQF(this->execute("EXISTS %s", _key.c_str()), SQF_RETURN_TYPE::STRING) != "";
}

bool RedisConnector::del(const std::string& _key) {
    return this->_DBExecToSQF(this->execute("DEL %s", _key.c_str()), SQF_RETURN_TYPE::NOTHING) != "";
}

std::string RedisConnector::ping() {
    return this->_DBExecToSQF(this->execute("PING"), SQF_RETURN_TYPE::NOTHING);
}


int RedisConnector::ttl(const std::string& _key) {
    return std::stoi(this->_DBExecToSQF(this->execute("TTL %s", _key.c_str()), SQF_RETURN_TYPE::STRING));
}

/*
std::string RedisConnector::lpopWithPrefix(const std::string& _prefix, const std::string& _key) {
    return this->_DBExecToSQF(this->execute("LPOP %s%s", _prefix.c_str(), _key.c_str()), SQF_RETURN_TYPE::STRING);
}

std::string RedisConnector::log(const std::string& _key, const std::string& _value) {
    char formatedTime[64];
    time_t t = time(0);
    struct tm * currentTime = localtime(&t);

    strftime(formatedTime, 64, "%Y-%m-%d %H:%M:%S ", currentTime);

    this->execute("LPUSH %s-LOG %s%s", _key.c_str(), formatedTime, _value.c_str());

    return this->_DBExecToSQF(this->execute("LTRIM %s-LOG 0 %d", _key.c_str(), this->config.logLimit), SQF_RETURN_TYPE::NOTHING).toArray();
}


std::string RedisConnector::increaseBancount() {
    return this->_DBExecToSQF(this->execute("INCR %s", "ahb-cnt"), SQF_RETURN_TYPE::STRING).toArray();
}
*/
