#ifndef __EPOCHLIB_H__
#define __EPOCHLIB_H__

#include <vector>
#include <shared_mutex>

#include <database/DBManager.hpp>
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
    
    std::shared_ptr<SteamAPI> steamApi = nullptr;
    
    /**
    * BattlEye RCON
    **/
    std::shared_ptr<RCON> rcon = nullptr;
    
    /**
    * Database Access
    **/
    std::unique_ptr<DBManager> dbManager;

    /**
    *  Helpers
    **/
    std::string __getBattlEyeGUID(uint64 steamId);    
    void __setupRCON(const rapidjson::Value& config);
    void __setupSteamAPI(const rapidjson::Value& config);

    void dbEntrypoint(std::string& out, int& outCode, const char *function, const char **args, int argsCnt);
    void beEntrypoint(std::string& out, int& outCode, const char *function, const char **args, int argsCnt);
    void callExtensionEntrypointByNumber(std::string& out, int& outCode, const char *function, const char **args, int argsCnt);

public:
    
    EpochServer();
    ~EpochServer();

    EpochServer(const EpochServer&) = delete;
    EpochServer& operator=(const EpochServer&) = delete;
    EpochServer(EpochServer&&) = delete;
    EpochServer& operator=(EpochServer&&) = delete;

    /**
    *  \brief Entrypoint that maps strings to functions for Arma's callExtension
    **/
    int callExtensionEntrypoint(char *output, int outputSize, const char *function, const char **args, int argsCnt);

    /** 
    *  UTIL FUNCTIONS
    **/

    /**
    *  \brief Get random string
    **/
    std::string getRandomString();

    /**
    *  \brief Generates a 24 Char unique id from current time and some random numbers
    *   
    */
    std::string getUniqueId();

    /**
    *  \brief Get md5 hashes of strings
    **/
    std::string getStringMd5(const std::string& stringToHash);
    std::vector<std::string> getStringMd5(const std::vector<std::string>& stringsToHash);

    /** 
    *  \brief Get current time as string of arma datetime array [%Y,%m,%d,%H,%M,%S]
    **/
    std::string getCurrentTime();
    
    /**
    *  \brief Log something to the extension logs
    **/
    void log(const std::string& log);
};

#endif //__EPOCHLIB_H__
