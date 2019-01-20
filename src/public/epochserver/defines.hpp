#ifndef __EPOCHLIB_DEF__
#define __EPOCHLIB_DEF__

#include <string>
#include <memory>



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
