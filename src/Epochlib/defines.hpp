#ifndef __EPOCHLIB_DEF__
#define __EPOCHLIB_DEF__

#include <string>
#include "Logger.hpp"

typedef unsigned char uint8;
typedef long long int int64;

struct EpochlibConfigDB {
    
    std::string dbType;
    
    std::string ip;
    unsigned short int port;
    std::string user;
    std::string password;
    std::string dbIndex;
    
    std::shared_ptr<Logger> logger;
    short int logAbuse;
    int logLimit;
};

struct EpochlibConfigSteamAPI {
    short int logging;
    std::string key;
    bool vacBanned;
    int vacMinNumberOfBans;
    int vacMaxDaysSinceLastBan;
    int playerAllowOlderThan;
};

struct EpochlibConfigBattlEye {
    std::string        ip;
    unsigned short int port;
    std::string        password;
    std::string        path;
};

struct EpochlibConfig {
    
    size_t outputSize;
    
    std::string battlEyePath;
    EpochlibConfigBattlEye battlEye;
    
    EpochlibConfigDB db;
    std::string hivePath;
    
    std::string profilePath;
    EpochlibConfigSteamAPI steamAPI;
    std::string instanceId;
    short int logAbuse;
    int logLimit;

};

struct EpochlibDBExecute {
    bool success;
    std::string message;
};
#endif