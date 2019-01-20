#ifndef __EPOCHLIB_H__
#define __EPOCHLIB_H__

#include <database/DBWorker.hpp>
#include <vector>

class EpochServer {
private:
    
    bool initialized = false;
    
    /* 
     * whitelists
     */
    std::mutex steamIdWhitelistMutex;
    std::vector<int64> steamIdWhitelist = {};

    /*
     * Database Access
     */
    std::vector<std::pair<std::string, DBWorker> > connections = {};

    /*
     *  Helpers
     */
    bool _loadConfig(const std::string& ConfigFilename);
    std::string _getBattlEyeGUID(int64 SteamId);    

public:
    
    EpochServer();
    ~EpochServer();

    int callExtensionEntrypoint(char *output, int outputSize, const char *function, const char **args, int argsCnt);

    /* 
     *  Get Config
     */
    std::string getConfig();

    /* 
     *  Initial player check
     *  64Bit Steam community id
     */
    std::string initPlayerCheck(int64 steamId);

    /* 
     *  Add ban with reason to bans.txt
     *  64Bit Steam community id
     *  Reason
     */
    std::string addBan(int64 steamId, const std::string& reason);

    /* 
     *  Add whitelisted string to publicvariable.txt
     *  String needs to be whitelisted
     */
    std::string updatePublicVariable(const std::vector<std::string>& whitelistStrings);
    std::string getRandomString(int stringCount);

    std::string getStringMd5(const std::vector<std::string>& stringsToHash);

    /* 
     *  Get current time
     */
    std::string getCurrentTime();

    /*
     *  Server Protection
     */
    std::string getServerMD5();
    
    /* 
     *  BE broadcast message 
     *  Message
     */
    void beBroadcastMessage (const std::string& Message);
    
    /* 
     *  BE kick
     *  PlayerUID
     * Message
     */
    void beKick (const std::string& PlayerUID, const std::string& Message);
    
    /* 
     *  BE ban
     *  PlayerUID
     *  Message
     *  Duration (minutes)
     */
    void beBan(const std::string& PlayerUID, const std::string& Message, const std::string& Duration);
    
    /* 
     *  BE shutdown 
     */
    void beShutdown();
    
    /* 
     *  BE lock / unlock
     */
    void beLock();
    void beUnlock();


    void log(const std::string& log);
};

#endif
