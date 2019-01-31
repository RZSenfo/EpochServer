#ifndef __EPOCHLIB_H__
#define __EPOCHLIB_H__

#include <vector>

#include <database/DBWorker.hpp>
#include <RCon/RCON.hpp>
#include <SteamAPI/SteamAPI.hpp>
#include <main.hpp>

#undef GetObject
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/error/error.h>
#include <rapidjson/istreamwrapper.h>


class EpochServer {
private:
    
    /** 
    * steam id check cache
    **/
    std::shared_mutex checkedSteamIdsMutex;
    std::vector<uint64> checkedSteamIds;
    
    /**
    * steam api config
    **/
    std::string steamApiKey;
    std::unique_ptr<SteamAPI> steamApi = nullptr;
    short steamApiLogLevel = 0;
    bool kickVacBanned = false;
    short minDaysSinceLastVacBan = 0;
    short maxVacBans = 1;
    int minAccountAge = 0; // in days

    /**
    * BattlEye RCON
    **/
    std::unique_ptr<RCON> rcon = nullptr;
    bool rconEnabled = false;
    std::string rconIp;
    unsigned short rconPort;
    std::string rconPassword;

    /**
    * Database Access
    **/
    std::vector<std::pair<std::string, std::shared_ptr<DBWorker> > > dbWorkers;

    /**
    *  Helpers
    **/
    std::string __getBattlEyeGUID(uint64 steamId);    
    void __setupBattlEye(const rapidjson::Value& config);
    void __setupSteamApi(const rapidjson::Value& config);
    void __setupConnection(const std::string& name, const rapidjson::Value& config);
    std::shared_ptr<DBWorker> __getDbWorker(const std::string& name);

public:
    
    EpochServer();
    ~EpochServer();

    /**
    *  \brief Entrypoint that maps strings to functions for Arma's callExtension
    **/
    int callExtensionEntrypoint(char *output, int outputSize, const char *function, const char **args, int argsCnt);

    /** 
    *  \brief Initial player check when joining
    *  \param steamId 64Bit Steam community id
    *  \return bool true if player was allowed to join, false if kicked
    **/
    bool initPlayerCheck(const std::string& steamId);

    /**
    *  \brief Get random string
    **/
    std::string getRandomString();

    /**
    *  \brief Get md5 hashes of strings
    **/
    std::string getStringMd5(const std::string& stringToHash);
    std::vector<std::string> getStringMd5(const std::vector<std::string>& stringsToHash);

    /** 
    *  \brief Get current time as string of arma datetime array [%Y,%m,%d,%H,%M,%S]
    **/
    std::string getCurrentTime();
    
    /* 
     *  \brief BE broadcast message 
     *  \param message
     */
    void beBroadcastMessage (const std::string& message);
    
    /** 
    *  \brief BE kick
    *
    *  Kicks a player
    *
    *  \param playerGUID
    **/
    void beKick (const std::string& playerGUID);
    
    /** 
    *  \brief BE ban
    *  \param playerGUID
    *  \param message
    *  \param duration (minutes)
    **/
    void beBan(const std::string& playerGUID, const std::string& message, int duration);
    
    /**
    *  \brief BE shutdown 
    **/
    void beShutdown();
    
    /** 
    *  \brief BE lock
    **/
    void beLock();
    
    /**
    *  \brief BE unlock
    **/
    void beUnlock();


    void log(const std::string& log);
};

#endif //__EPOCHLIB_H__
