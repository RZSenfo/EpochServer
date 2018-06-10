#include "RedisConnector.hpp"

#include <sstream>

#ifdef WIN32
#include "hiredis.h"
#define INCL_WINSOCK_API_PROTOTYPES 0
#include <WinSock2.h>
#define IS_WINDOWS_PLATFORM
#endif

#ifdef IS_WINDOWS_PLATFORM
#else
//#include "../../deps/redis/deps/hiredis/hiredis.h"
#include <hiredis.h>
#include <sys/time.h>
#include <string.h>
#endif


RedisConnector::RedisConnector() {
    this->context = NULL;
}

RedisConnector::~RedisConnector() {
    if (this->context != NULL) {
        redisFree(this->context);
    }
}

bool RedisConnector::init(EpochlibConfigDB _config) {

    this->config = _config;

    // Setup regex validation
    const char *error;
    int erroffset;
    std::stringstream regexString;
    regexString << "(?(DEFINE)";
    regexString << "(?<boolean>true|false)";
    regexString << "(?<number>-?(?=[1-9]|0(?!\\d))\\d+(\\.\\d+)?([eE][+\\-]?\\d+)?)";
    regexString << "(?<string>\"([^\"]*|\\\\\\\\[\"\\\\\\\\bfnrt\\/]|\\\\u[0-9a-f]{4})*\")";
    regexString << "(?<array>\\[(?:(?&container)(?:,(?&container))*)?\\s*\\])";
    regexString << "(?<container>\\s*(?:(?&boolean)|(?&number)|(?&string)|(?&array))\\s*)";
    regexString << ")";
    regexString << "\\A(?&array)\\Z";
    this->setValueRegex = pcre_compile(regexString.str().c_str(), PCRE_CASELESS | PCRE_DOTALL | PCRE_EXTENDED, &error, &erroffset, NULL);
    if (this->setValueRegex == NULL) {
        this->config.logger->log("PCRE compile error: " + std::string(error, erroffset));
        return false;
    }

    this->_reconnect(false);

    return true;
}

EpochlibDBExecute RedisConnector::execute(const char *format, ...) {
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
            this->config.logger->log("[Redis] Error command " + std::string(reply->str));
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

void RedisConnector::_reconnect(bool _force) {
    // Security context lock
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
                this->config.logger->log("[Redis] " + std::string(this->context->errstr));
            }

            retries++;
        } while (this->context == NULL || (this->context->err & (REDIS_ERR_IO || REDIS_ERR_EOF) && retries < REDISCONNECTOR_MAXCONNECTION_RETRIES));

        /* Too many retries -> exit server with log */
        if (retries == REDISCONNECTOR_MAXCONNECTION_RETRIES) {
            this->config.logger->log("[Redis] Server not reachable");
            exit(1);
        }

        /* Password given -> AUTH */
        if (!this->config.password.empty()) {
            redisReply *authReply = NULL;

            while (authReply == NULL) {
                authReply = (redisReply *)redisCommand(this->context, "AUTH %s", this->config.password.c_str());
            }
            if (authReply->type == REDIS_REPLY_STRING) {
                if (strcmp(authReply->str, "OK") == 0) {
                    this->config.logger->log("[Redis] Could not authenticate: " + std::string(authReply->str));
                }
            }

            freeReplyObject(authReply);
        }

        /* Database index given -> change database */
        int dbIndex = std::stoi(this->config.dbIndex);
        if (dbIndex > 0) {
            redisReply *selectReply = NULL;

            while (selectReply == NULL) {
                selectReply = (redisReply *)redisCommand(this->context, "SELECT %d", dbIndex);
            }
            if (selectReply->type == REDIS_REPLY_STRING) {
                if (strcmp(selectReply->str, "OK") == 0) {
                    this->config.logger->log("[Redis] Could not change database: " + std::string(selectReply->str));
                }
            }

            freeReplyObject(selectReply);
        }
    }

    // Unlock
    this->contextMutex.unlock();
}

std::string RedisConnector::getRange(const std::string& _key, const std::string& _value, const std::string& _value2) {
    EpochlibDBExecute value = this->execute("GETRANGE %s %s %s", _key.c_str(), _value.c_str(), _value2.c_str());
    if (value.success == 1) {
        
        SQF returnSqf;
        returnSqf.push_number(SQF_RETURN_STATUS::SUCCESS);
        
        
        /*
        std::string output = value.message;
        std::stringstream outputSteam;
        for (std::string::iterator it = output.begin(); it != output.end(); ++it) {
            if (*it == '\'') {
                outputSteam << '\'';
            }
            outputSteam << *it;
        }
        */
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
}

std::string RedisConnector::get(const std::string& _key) {


    auto temp = this->execute("GET %s", _key.c_str());
    
    // GET success proceed
    if (temp.success == 1) {
        
        SQF returnSqf;
        returnSqf.push_number(SQF_RETURN_STATUS::SUCCESS); // single row

        /*
        std::string output = temp.message;
        
        std::stringstream outputSteam;
        for (std::string::iterator it = output.begin(); it != output.end(); ++it) {
            if (*it == '\'') {
                outputSteam << '\'';
            }
            outputSteam << *it;
        }

        auto result = outputSteam.str();
        */
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

}

std::string RedisConnector::getTtl(const std::string& _key) {

    // No temp GET found -> GET new one
    auto temp = this->execute("GET %s", _key.c_str());

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
        
        /*
        std::string output = temp.message;
        std::stringstream outputSteam;
        for (std::string::iterator it = output.begin(); it != output.end(); ++it) {
            if (*it == '\'') {
                outputSteam << '\'';
            }
            outputSteam << *it;
        }
        returnSqf.push_str(outputSteam.str().c_str(), 0);
        */

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
    
}

std::string RedisConnector::set(const std::string& _key, const std::string& _value) {
    // _value not used atm    
    int regexReturnCode = pcre_exec(this->setValueRegex, NULL, _value.c_str(), _value.length(), 0, 0, NULL, NULL);
    if (regexReturnCode == 0) {
        return this->_DBExecToSQF(this->execute("SET %s %s", _key.c_str(), _value.c_str()), SQF_RETURN_TYPE::NOTHING).toArray();
    }
    else {

        if (this->config.logAbuse > 0) {
            this->config.logger->log("[Abuse] SETEX key " + _key + " does not match the allowed syntax!" + (this->config.logAbuse > 1 ? "\n" + _value : ""));
        }

        return SQF::RET_FAIL();
    }
}

std::string RedisConnector::setex(const std::string& _key, const std::string& _ttl, const std::string& _value) {
    int regexReturnCode = pcre_exec(this->setValueRegex, NULL, _value.c_str(), _value.length(), 0, 0, NULL, NULL);
    if (regexReturnCode == 0) {
        return this->_DBExecToSQF(this->execute("SETEX %s %s %s", _key.c_str(), _ttl.c_str(), _value.c_str()), SQF_RETURN_TYPE::NOTHING).toArray();
    }
    else {
        if (this->config.logAbuse > 0) {
            this->config.logger->log("[Abuse] SETEX key " + _key + " does not match the allowed syntax!" + (this->config.logAbuse > 1 ? "\n" + _value : ""));
        }

        return SQF::RET_FAIL();
    }
}

std::string RedisConnector::expire(const std::string& _key, const std::string& _ttl) {
    return this->_DBExecToSQF(this->execute("EXPIRE %s %s", _key.c_str(), _ttl.c_str()), SQF_RETURN_TYPE::NOTHING).toArray();
}

std::string RedisConnector::setbit(const std::string& _key, const std::string& _value, const std::string& _value2) {
    return this->_DBExecToSQF(this->execute("SETBIT %s %s %s", _key.c_str(), _value.c_str(), _value2.c_str()), SQF_RETURN_TYPE::NOTHING).toArray();
}

std::string RedisConnector::getbit(const std::string& _key, const std::string& _value) {
    return this->_DBExecToSQF(this->execute("GETBIT %s %s", _key.c_str(), _value.c_str()), SQF_RETURN_TYPE::NOTHING).toArray();
}

std::string RedisConnector::exists(const std::string& _key) {
    return this->_DBExecToSQF(this->execute("EXISTS %s", _key.c_str()), SQF_RETURN_TYPE::STRING).toArray();
}

std::string RedisConnector::del(const std::string& _key) {
    return this->_DBExecToSQF(this->execute("DEL %s", _key.c_str()), SQF_RETURN_TYPE::NOTHING).toArray();
}

std::string RedisConnector::ping() {
    return this->_DBExecToSQF(this->execute("PING"), SQF_RETURN_TYPE::NOTHING).toArray();
}

std::string RedisConnector::lpopWithPrefix(const std::string& _prefix, const std::string& _key) {
    return this->_DBExecToSQF(this->execute("LPOP %s%s", _prefix.c_str(), _key.c_str()), SQF_RETURN_TYPE::STRING).toArray();
}

std::string RedisConnector::ttl(const std::string& _key) {
    return this->_DBExecToSQF(this->execute("TTL %s", _key.c_str()), SQF_RETURN_TYPE::STRING).toArray();
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
