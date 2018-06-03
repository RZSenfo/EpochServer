/* Default Epochlib defines */
#include "defines.hpp"

#include "Logger.hpp"
#include "RedisConnector.hpp"
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

#define PCRE_STATIC 1
#include <pcre.h>

#ifndef __EPOCHLIB_H__
#define __EPOCHLIB_H__

#define EPOCHLIB_SQF_NOTHING 0
#define EPOCHLIB_SQF_STRING 1
#define EPOCHLIB_SQF_ARRAY 2

#define EPOCHLIB_SQF_RET_FAIL 0
#define EPOCHLIB_SQF_RET_SUCESS 1
#define EPOCHLIB_SQF_RET_CONTINUE 2

#define EPOCHLIB_SERVERMD5 "8497e70dafab88ea432338fee8c86579" //"426a526f91eea855dc889d21205e574c"

/* !!! TESTING DO NOT ENABLE IN PRODUCTION !!! */
//#define EPOCHLIB_TEST
#define EPOCHLIB_DISABLE_OFFICALCHECK

class Epochlib {
private:
	bool initialized;
	EpochlibConfig config;

	Logger *logger;

	bool _fileExist(std::string FileName);
	bool _loadConfig(std::string ConfigFilename);
	std::string _getBattlEyeGUID(int64 SteamId);
	bool _isOffialServer();

	RedisConnector *redis;
	SQF _redisExecToSQF(const EpochlibRedisExecute& RedisExecute, int ForceMessageOutputType);
	EpochlibRedisExecute tempGet;
	pcre *setValueRegex;
	std::mutex tempSetMutex;
	std::string tempSet;
    /*
    */


	std::mutex steamIdWhitelistMutex;
	std::vector<int64> steamIdWhitelist;

public:
	Epochlib(std::string ConfigPath, std::string ProfilePath, int OutputSize);
	~Epochlib();

	/* Get Config
	*/
	std::string getConfig();

	/* Initial player check
	* 64Bit Steam community id
	*/
	std::string initPlayerCheck(int64 SteamId);

	/* Add ban with reason to bans.txt
	* 64Bit Steam community id
	* Reason
	*/
	std::string addBan(int64 SteamId, const std::string& Reason);

	/* Add whitelisted string to publicvariable.txt
	* String needs to be whitelisted
	*/
	std::string updatePublicVariable(const std::vector<std::string>& WhitelistStrings);
	std::string getRandomString(int StringCount);

	/* Increase bancount
	*/
	std::string increaseBancount();
	std::string getStringMd5(const std::vector<std::string>& StringsToHash);
	

	/* Get current time
	*/
	std::string getCurrentTime();

	/* Redis GET
	* Key
	*/
	std::string get(const std::string& Key);
	std::string getRange(const std::string& Key, const std::string& Value, const std::string& Value2);
	std::string getTtl(const std::string& Key);
	std::string getbit(const std::string& Key, const std::string& Value);
	std::string exists(const std::string& Key);

	/* Redis SET / SETEX
	*/
	// std::string setTemp(std::string Key, std::string Value, std::string Value2);
	std::string set(const std::string& Key, const std::string& Value, const std::string& Value2);
	std::string setex(const std::string& Key, const std::string& Value, const std::string& Value2, const std::string& Value3);
	std::string expire(const std::string& Key, const std::string& TTL);
	std::string setbit(const std::string& Key, const std::string& Value, const std::string& Value2);

	/* Redis DEL
	* Key
	*/
	std::string del(const std::string& Key);

	/* Redis PING
	*/
	std::string ping();

	/* Redis LPOP with a given prefix
	*/
	std::string lpopWithPrefix(const std::string& Prefix, const std::string& Key);

	/* Redis TTL
	* Key
	*/
	std::string ttl(const std::string& Key);

	std::string log(const std::string& Key, const std::string& Value);

	std::string getServerMD5();
	
	/* BE broadcast message 
	* Message
	*/
	void beBroadcastMessage (const std::string& Message);
	
	/* BE kick
	* PlayerUID
	* Message
	*/
	void beKick (const std::string& PlayerUID, const std::string& Message);
	
	/* BE ban
	* PlayerUID
	* Message
	* Duration (minutes)
	*/
	void beBan(const std::string& PlayerUID, const std::string& Message, const std::string& Duration);
	
	/* BE shutdown 
	*/
	void beShutdown();
	
	/* BE lock / unlock
	*/
	void beLock();
	void beUnlock();
};

#endif