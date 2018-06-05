#ifndef __EPOCHLIB_H__
#define __EPOCHLIB_H__

/* Default Epochlib defines */
#include "defines.hpp"

#include "Logger.hpp"
#include "RedisConnector.hpp"
#include "MySQLConnector.hpp"
#include "SQF.hpp"
#include "../SteamAPI/SteamAPI.hpp"
#include "../external/md5.hpp"
#include "../external/ConfigFile.hpp"
#include <string>
#include <sstream>
#include <fstream>
#include <ctime>
#include <mutex>
#include <regex>

#define EPOCHLIB_SERVERMD5 "8497e70dafab88ea432338fee8c86579" //"426a526f91eea855dc889d21205e574c"

/* !!! TESTING DO NOT ENABLE IN PRODUCTION !!! */
//#define EPOCHLIB_TEST
#define EPOCHLIB_DISABLE_OFFICALCHECK

class Epochlib {
private:
	
    bool initialized;
	EpochlibConfig config;

	std::shared_ptr<Logger> logger;

    //Helper
	bool _fileExist(const std::string& FileName);
	bool _loadConfig(const std::string& ConfigFilename);
	std::string _getBattlEyeGUID(int64 SteamId);
	bool _isOffialServer();
    	
    //MySQL specifics
    // -

    //Antihack
	std::mutex steamIdWhitelistMutex;
	std::vector<int64> steamIdWhitelist;

public:
	
    Epochlib(std::string configPath, std::string profilePath, int outputSize);
	~Epochlib();

    /*
     * Database Access
     */
    std::unique_ptr<DBConnector> db;

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
};

#endif