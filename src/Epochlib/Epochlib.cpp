#include "Epochlib.hpp"
#include "../SteamAPI/SteamAPI.hpp"
#include "../BattlEye/BEClient.hpp"

#include <cstdlib>
#include <ctime>
#include <cctype>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <fstream>

bool iequals(const std::string& string1, const std::string& string2) {
    return  string1.size() == string2.size()
        &&
        std::equal(
            string1.begin(),
            string1.end(),
            string2.begin(),
            [](const char& c1, const char& c2) {
        return (c1 == c2 || std::toupper(c1) == std::toupper(c2));
    });
}

Epochlib::Epochlib(std::string _configPath, std::string _profilePath, int _outputSize) {
	
    this->initialized = false;
	
    this->config.hivePath = _configPath;
	this->config.profilePath = _profilePath;
	this->config.outputSize = _outputSize;

	// Init random
	std::srand(std::time(0));

	this->logger = std::make_shared<Logger>(this->config.profilePath + "/EpochServer.log");

#ifndef EPOCHLIB_TEST
#ifndef EPOCHLIB_DISABLE_OFFICALCHECK
	/* Log & exit if server does not use official server files */
	if (!this->_isOffialServer()) {
		this->logger->log("Wrong server files");
		exit(1);
	}
#endif
#endif

    //Try config load
	if (
        this->_loadConfig(_configPath + "/EpochServer.ini") 
        || 
        this->_loadConfig(_profilePath + "/EpochServer.ini") 
        || 
	    this->_loadConfig(_configPath + "/epochserver.ini")
    ) {
		this->initialized = true;
	}
	else {
		this->logger->log("EpochServer.ini not found in (" + _configPath + ", " + _profilePath + ")");
		std::exit(1);
	}

    
#ifndef EPOCHLIB_TEST
    if (iequals(this->config.db.dbType, "Redis")) {
        this->db = std::make_unique<RedisConnector>();
    }
    else if (iequals(this->config.db.dbType, "MySQL")) {
        this->db = std::make_unique<MySQLConnector>();
    }
    
    if (this->db->init(this->config.db)) {
        this->logger->log(this->config.db.dbType + " Database connected");
    }
    else {
        this->logger->log("Could not initialize Database. Check the config!");
        std::exit(1);
    }


#endif

}
Epochlib::~Epochlib() {

}

std::string Epochlib::getConfig() {
	SQF returnSqf;

	returnSqf.push_str(this->config.instanceId.c_str());
	returnSqf.push_number(this->config.steamAPI.key.empty() ? 0 : 1);

	return returnSqf.toArray();
}

std::string Epochlib::initPlayerCheck(int64 _steamId) {
	bool proceeded = false;

	// Not in whitelist
	if (std::find(this->steamIdWhitelist.begin(), this->steamIdWhitelist.end(), _steamId) == this->steamIdWhitelist.end()) {

		// SteamAPI key is not empty
		if (!this->config.steamAPI.key.empty()) {
			SteamAPI steamAPI(this->config.steamAPI.key);

			rapidjson::Document document;
			std::stringstream steamIds;
			steamIds << _steamId;

			// VAC check
			if (!proceeded && steamAPI.GetPlayerBans(steamIds.str(), &document)) {
				if (this->config.steamAPI.logging >= 2) {
					std::stringstream log;
					log << "[SteamAPI] VAC check " << _steamId << std::endl;
					log << "- VACBanned: " << (document["players"][0]["VACBanned"].GetBool() ? "true" : "false") << std::endl;
					log << "- DaysSinceLastBan: " << document["players"][0]["DaysSinceLastBan"].GetInt() << std::endl;
					log << "- NumberOfVACBans: " << document["players"][0]["NumberOfVACBans"].GetInt();
					this->logger->log(log.str());
				}

				if (!proceeded && this->config.steamAPI.vacBanned && document["players"][0]["VACBanned"].GetBool()) {
					if (this->config.steamAPI.logging >= 1) {
						std::stringstream log;
						log << "[SteamAPI] VAC ban " << _steamId << std::endl;
						log << "- VACBanned: " << (document["players"][0]["VACBanned"].GetBool() ? "true" : "false");
						this->logger->log(log.str());
					}

					this->addBan(_steamId, "VAC Ban");
					proceeded = true;
				}
				if (!proceeded && this->config.steamAPI.vacMaxDaysSinceLastBan > 0 && document["players"][0]["DaysSinceLastBan"].GetInt() < this->config.steamAPI.vacMaxDaysSinceLastBan) {
					if (this->config.steamAPI.logging >= 1) {
						std::stringstream log;
						log << "[SteamAPI] VAC ban " << _steamId << std::endl;
						log << "- DaysSinceLastBan: " << document["players"][0]["DaysSinceLastBan"].GetInt();
						this->logger->log(log.str());
					}

					this->addBan(_steamId, "VAC Ban");
					proceeded = true;
				}
				if (!proceeded && this->config.steamAPI.vacMinNumberOfBans > 0 && document["players"][0]["NumberOfVACBans"].GetInt() >= this->config.steamAPI.vacMinNumberOfBans) {
					if (this->config.steamAPI.logging >= 1) {
						std::stringstream log;
						log << "[SteamAPI] VAC ban " << _steamId << std::endl;
						log << "- NumberOfVACBans: " << document["players"][0]["NumberOfVACBans"].GetInt();
						this->logger->log(log.str());
					}

					this->addBan(_steamId, "VAC Ban");
					proceeded = true;
				}
			}

			// Player check
			if (!proceeded && steamAPI.GetPlayerSummaries(steamIds.str(), &document)) {
				if (this->config.steamAPI.logging >= 2) {
					std::stringstream log;
					log << "[SteamAPI] Player check " << _steamId << std::endl;
					log << "- timecreated: " << document["response"]["players"][0]["timecreated"].GetInt();
					this->logger->log(log.str());
				}

				if (!proceeded && this->config.steamAPI.playerAllowOlderThan > 0) {
					std::time_t currentTime = std::time(nullptr);

					if ((currentTime - document["response"]["players"][0]["timecreated"].GetInt()) < this->config.steamAPI.playerAllowOlderThan) {
						if (this->config.steamAPI.logging >= 1) {
							std::stringstream log;
							log << "[SteamAPI] Player ban " << _steamId << std::endl;
							log << "- timecreated: " << document["response"]["players"][0]["timecreated"].GetInt() << std::endl;
							log << "- current: " << currentTime;
							this->logger->log(log.str());
						}

						this->addBan(_steamId, "New account filter");
						proceeded = true;
					}
				}
			}
		}

		// Not proceeded -> fine
		if (!proceeded) {
			this->steamIdWhitelistMutex.lock();
			this->steamIdWhitelist.push_back(_steamId);
			this->steamIdWhitelistMutex.unlock();
		}
	}

	return "";
}

std::string Epochlib::addBan(int64 _steamId, const std::string& _reason) {
	std::string battleyeGUID = this->_getBattlEyeGUID(_steamId);
	std::string bansFilename = this->config.battlEyePath + "/bans.txt";
	SQF returnSQF;

	std::ofstream bansFile(bansFilename, std::ios::app);
	if (bansFile.good()) {
		bansFile << battleyeGUID << " -1 " << _reason << std::endl;
		bansFile.close();

                this->logger->log("BEClient: try to connect " + this->config.battlEye.ip);
                BEClient bec     (this->config.battlEye.ip.c_str(), this->config.battlEye.port);
                bec.sendLogin    (this->config.battlEye.password.c_str());
                bec.readResponse (BE_LOGIN);
                if (bec.isLoggedIn()) {
                    this->logger->log("BEClient: logged in!");
                    bec.sendCommand  ("loadBans");
                    bec.readResponse (BE_COMMAND);
                
                    bec.sendCommand  ("players");
                    bec.readResponse (BE_COMMAND);
                        
                    int playerSlot = bec.getPlayerSlot (battleyeGUID);
                    if (playerSlot >= 0) {
                        std::stringstream ss;
                        ss << "ban " << playerSlot << " 0 " << _reason;
                        bec.sendCommand  (ss.str().c_str());
                        bec.readResponse (BE_COMMAND);
                    }
                } else {
                    this->logger->log("BEClient: login failed!");
                }
                bec.disconnect();

		returnSQF.push_str("1");
		returnSQF.push_str(battleyeGUID.c_str());
	}
	else {
		bansFile.close();
		returnSQF.push_str("0");
	}

	return returnSQF.toArray();
}

std::string Epochlib::updatePublicVariable(const std::vector<std::string>& _whitelistStrings) {
	std::string pvFilename = this->config.battlEyePath + "/publicvariable.txt";
	std::string pvContent = "";
	bool pvFileOriginalFound = false;
	SQF returnSQF;

	// Try to read the original file content
	std::ifstream pvFileOriginal(pvFilename + ".original");
	if (pvFileOriginal.good()) {
		// Jump to the end
		pvFileOriginal.seekg(0, std::ios::end);
		// Allocate memory
		pvContent.reserve((unsigned int)pvFileOriginal.tellg());
		// Jump to the begin
		pvFileOriginal.seekg(0, std::ios::beg);
		// Assing content
		pvContent.assign(std::istreambuf_iterator<char>(pvFileOriginal), std::istreambuf_iterator<char>());

		pvFileOriginalFound = true;
	}
	pvFileOriginal.close();

	// Original file not found
	if (!pvFileOriginalFound) {
		std::ifstream pvFileCurrent(pvFilename);
		if (pvFileCurrent.good()) {
			// Jump to the end
			pvFileCurrent.seekg(0, std::ios::end);
			// Allocate memory
			pvContent.reserve((unsigned int)pvFileCurrent.tellg());
			// Jump to the begin
			pvFileCurrent.seekg(0, std::ios::beg);
			// Assing content
			pvContent.assign(std::string(std::istreambuf_iterator<char>(pvFileCurrent), std::istreambuf_iterator<char>()));

			pvFileCurrent.close();
		}
		else {
			pvFileCurrent.close();
			returnSQF.push_str("0");
			this->logger->log("publicvariable.txt not found in " + this->config.battlEyePath);
			return returnSQF.toArray();
		}

		// Copy content to the original file
		std::ofstream pvFileOriginalNew(pvFilename + ".original");
		if (pvFileOriginalNew.good()) {
			pvFileOriginalNew << pvContent;
			pvFileOriginalNew.close();
		}
		else {
			pvFileOriginalNew.close();
			returnSQF.push_str("0");
			return returnSQF.toArray();
		}
	}

	// write new pvFile
	std::ofstream pvFileNew(pvFilename);
	if (pvFileNew.good()) {
		pvFileNew << pvContent;

		for (auto it = _whitelistStrings.begin(); it != _whitelistStrings.end(); it++) {
			pvFileNew << " !=\"" << *it << "\"";
		}

		returnSQF.push_str("1");
	}
	else {
		returnSQF.push_str("0");
	}
	pvFileNew.close();
	
	this->logger->log("BEClient: try to connect " + this->config.battlEye.ip);
	BEClient bec     (this->config.battlEye.ip.c_str(), this->config.battlEye.port);
	bec.sendLogin    (this->config.battlEye.password.c_str());
	bec.readResponse (BE_LOGIN);
	if (bec.isLoggedIn()) {
	    this->logger->log("BEClient: logged in!");
	    bec.sendCommand  ("loadEvents");
	    bec.readResponse (BE_COMMAND);
	} else {
	    this->logger->log("BEClient: login failed!");
	}
	bec.disconnect();

	return returnSQF.toArray();
}
std::string Epochlib::getRandomString(int _stringCount) {
	SQF returnSQF;
	std::vector<std::string> randomStrings;

	// Define char pool
	const char charPool[] = "abcdefghijklmnopqrstuvwxyz";
	int charPoolSize = sizeof(charPool) - 1;

	for (int stringCounter = 0; stringCounter < _stringCount; stringCounter++) {
		std::string randomString;
		int stringLength = (std::rand() % 5) + 5; //random string size between 5-10

		for (int i = 0; i < stringLength; i++) {
			randomString += charPool[std::rand() % charPoolSize];
		}

		// Build unique string array
		if (std::find(randomStrings.begin(), randomStrings.end(), randomString) == randomStrings.end() && randomString.find("god") == std::string::npos) {
			randomStrings.push_back(randomString);
		}
		else {
			stringCounter--;
		}
	}

	if (_stringCount == 1 && randomStrings.size() == 1) {
		return randomStrings.at(0);
	}
	else {
		for (std::vector<std::string>::iterator it = randomStrings.begin(); it != randomStrings.end(); ++it) {
			returnSQF.push_str(it->c_str());
		}

		return returnSQF.toArray();
	}
}

std::string Epochlib::getStringMd5(const std::vector<std::string>& _stringsToHash) {
	SQF returnSQF;
	
	for (auto it = _stringsToHash.begin(); it != _stringsToHash.end(); ++it) {
		MD5 md5 = MD5(*it);
		returnSQF.push_str(md5.hexdigest().c_str());
	}

	return returnSQF.toArray();
}

std::string Epochlib::getCurrentTime() {
	SQF returnSQF;
	char buffer[8];
	size_t bufferSize;

	time_t t = time(0);
	struct tm * currentTime = localtime(&t);

	bufferSize = strftime(buffer, 8, "%Y", currentTime);
	returnSQF.push_number(buffer, bufferSize);

	bufferSize = strftime(buffer, 8, "%m", currentTime);
	returnSQF.push_number(buffer, bufferSize);

	bufferSize = strftime(buffer, 8, "%d", currentTime);
	returnSQF.push_number(buffer, bufferSize);

	bufferSize = strftime(buffer, 8, "%H", currentTime);
	returnSQF.push_number(buffer, bufferSize);

	bufferSize = strftime(buffer, 8, "%M", currentTime);
	returnSQF.push_number(buffer, bufferSize);

	bufferSize = strftime(buffer, 8, "%S", currentTime);
	returnSQF.push_number(buffer, bufferSize);

	return returnSQF.toArray();
}

std::string Epochlib::getServerMD5() {

	std::string serverMD5;
	std::string addonPath = this->config.hivePath + "/addons/a3_epoch_server.pbo";
	FILE *srvFile = fopen(addonPath.c_str(), "rb");
	if (srvFile != NULL) {
		MD5 md5Context;
		int bytes;
		unsigned char data[1024];

		while ((bytes = fread(data, 1, 1024, srvFile)) != 0) {
			md5Context.update(data, bytes);
		}

		md5Context.finalize();
		serverMD5 = md5Context.hexdigest();
	}
	else {
		serverMD5 = addonPath;
	}
	
	return serverMD5;
}

std::string Epochlib::_getBattlEyeGUID(int64 _steamId) {
	uint8 i = 0;
	uint8 parts[8] = { 0 };

	do
	{
		parts[i++] = _steamId & 0xFF;
	} while (_steamId >>= 8);

	std::stringstream bestring;
	bestring << "BE";
	for (unsigned int i = 0; i < sizeof(parts); i++) {
		bestring << char(parts[i]);
	}

	MD5 md5 = MD5(bestring.str());

	return md5.hexdigest();
}

bool Epochlib::_fileExist(const std::string& _filename) {
	
    std::ifstream file(_filename.c_str());
    bool exists = file.good();
	file.close();

	return exists;
}

bool Epochlib::_loadConfig(const std::string& configFilename) {
	
    if (this->_fileExist(configFilename)) {
		
        ConfigFile configFile(configFilename);

		// EpochServer config
		this->config.battlEyePath   = (std::string)configFile.Value("EpochServer", "BattlEyePath", (this->config.profilePath.length() > 0 ? this->config.profilePath + "/battleye" : ""));
		this->config.instanceId     = (std::string)configFile.Value("EpochServer", "InstanceID", "NA123");
		this->config.logAbuse       = (unsigned short int)configFile.Value("EpochServer", "LogAbuse", 0);
		this->config.logLimit       = (unsigned short int)configFile.Value("EpochServer", "LogLimit", 999);
		
		this->config.battlEye.ip    = (std::string)configFile.Value("EpochServer", "IP", "127.0.0.1");
		this->config.battlEye.port  = (unsigned short int)configFile.Value("EpochServer", "Port", 2306);
		this->config.battlEye.password = (std::string)configFile.Value("EpochServer", "Password", "");
		this->config.battlEye.path  = this->config.battlEyePath;

        //DB Config
        auto dbType = this->config.db.dbType   = (std::string)configFile.Value("Database", "Type", "Redis");

		this->config.db.ip       = (std::string)configFile.Value(dbType, "IP", "127.0.0.1");
		this->config.db.port     = (unsigned short int)configFile.Value(dbType, "Port", dbType == "Redis" ? 6379 : 3306);
        this->config.db.user     = (std::string)configFile.Value(dbType, "User", dbType == "Redis" ? "" : "root");
        this->config.db.password = (std::string)configFile.Value(dbType, "Password", "");
		this->config.db.dbIndex  = (std::string)configFile.Value(dbType, "DB", dbType == "Redis" ? "0" : "epoch");
		this->config.db.logger   = this->logger;

		// SteamApi
		this->config.steamAPI.logging                = configFile.Value("SteamAPI", "Logging", 0);
		this->config.steamAPI.key                    = (std::string)configFile.Value("SteamAPI", "Key", "");
		this->config.steamAPI.vacBanned              = configFile.Value("SteamAPI", "VACBanned", 0) > 0;
		this->config.steamAPI.vacMinNumberOfBans     = configFile.Value("SteamAPI", "VACMinimumNumberOfBans", 0);
		this->config.steamAPI.vacMaxDaysSinceLastBan = configFile.Value("SteamAPI", "VACMaximumDaysSinceLastBan", 0);
		this->config.steamAPI.playerAllowOlderThan   = configFile.Value("SteamAPI", "PlayerAllowOlderThan", 0);

		return true;
	}
	else {
		return false;
	}
}

bool Epochlib::_isOffialServer() {
	return this->getServerMD5() == EPOCHLIB_SERVERMD5 ? true : false;
}

// Battleye Integration

void Epochlib::beBroadcastMessage (const std::string& msg) {
	if (msg.empty()) 
	    return;

        this->logger->log("BEClient: try to connect " + this->config.battlEye.ip);
	BEClient bec     (this->config.battlEye.ip.c_str(), this->config.battlEye.port);	
	bec.sendLogin    (this->config.battlEye.password.c_str());
	bec.readResponse (BE_LOGIN);
	if (bec.isLoggedIn()) {
	    this->logger->log("BEClient: logged in!");
	    std::stringstream ss;
	    ss << "say -1 " << msg;
	    bec.sendCommand  (ss.str().c_str());
	    bec.readResponse (BE_COMMAND);
	} else {
	    this->logger->log("BEClient: login failed!");
	}
	bec.disconnect   ();
}

void Epochlib::beKick (const std::string& playerUID, const std::string& msg) {
	if (playerUID.empty() || msg.empty()) 
	    return;
	
	this->logger->log("BEClient: try to connect " + this->config.battlEye.ip);
	BEClient bec     (this->config.battlEye.ip.c_str(), this->config.battlEye.port);
	bec.sendLogin    (this->config.battlEye.password.c_str());
	bec.readResponse (BE_LOGIN);
	if (bec.isLoggedIn()) {
	    this->logger->log("BEClient: logged in!");
	    bec.sendCommand  ("players");
	    bec.readResponse (BE_COMMAND);
        
	    std::string playerGUID = this->_getBattlEyeGUID (atoll(playerUID.c_str()));
	    int playerSlot         = bec.getPlayerSlot (playerGUID);
	    if (playerSlot < 0) {
	        bec.disconnect();
                return;
            }
	
	    std::stringstream ss;
	    ss << "kick " << playerSlot << ' ' << msg;
	    bec.sendCommand  (ss.str().c_str());
	    bec.readResponse (BE_COMMAND);
        } else {
            this->logger->log("BEClient: login failed!");
        }
	bec.disconnect   ();
}

void Epochlib::beBan(const std::string& playerUID, const std::string& msg, const std::string& duration) {
    if (playerUID.empty() || msg.empty())
        return;
            
    this->logger->log("BEClient: try to connect " + this->config.battlEye.ip);
    BEClient bec     (this->config.battlEye.ip.c_str(), this->config.battlEye.port);
    bec.sendLogin    (this->config.battlEye.password.c_str());
    bec.readResponse (BE_LOGIN);
    if (bec.isLoggedIn()) {
        this->logger->log("BEClient: logged in!");
        bec.sendCommand  ("players");
        bec.readResponse (BE_COMMAND);

        std::string playerGUID = this->_getBattlEyeGUID (atoll(playerUID.c_str()));
        int playerSlot         = bec.getPlayerSlot (playerGUID);
        if (playerSlot < 0) {
            bec.disconnect();
            return;
        }
        
        std::stringstream ss;
        ss << "ban " << playerSlot << ' ' << (duration.empty() ? "-1" : duration) << ' ' << msg;
        bec.sendCommand  (ss.str().c_str());
        bec.readResponse (BE_COMMAND);
    } else {
        this->logger->log("BEClient: login failed!");
    }
    bec.disconnect   ();
}

void Epochlib::beShutdown() {
    this->logger->log("BEClient: try to connect " + this->config.battlEye.ip);
    BEClient bec     (this->config.battlEye.ip.c_str(), this->config.battlEye.port);
    bec.sendLogin    (this->config.battlEye.password.c_str());
    bec.readResponse (BE_LOGIN);
    if (bec.isLoggedIn()) {
        this->logger->log("BEClient: logged in!");
        bec.sendCommand  ("#shutdown");
        bec.readResponse (BE_COMMAND);
    } else {
        this->logger->log("BEClient: login failed!");
    }	
    bec.disconnect   ();
}

void Epochlib::beLock() {
    this->logger->log("BEClient: try to connect " + this->config.battlEye.ip);
    BEClient bec(this->config.battlEye.ip.c_str(), this->config.battlEye.port);
    bec.sendLogin(this->config.battlEye.password.c_str());
    bec.readResponse(BE_LOGIN);
    if (bec.isLoggedIn()) {
        this->logger->log("BEClient: logged in!");
        bec.sendCommand("#lock");
        bec.readResponse(BE_COMMAND);
    } else {
        this->logger->log("BEClient: login failed!");
    }
    bec.disconnect();
}

void Epochlib::beUnlock() {
    this->logger->log("BEClient: try to connect " + this->config.battlEye.ip);
    BEClient bec(this->config.battlEye.ip.c_str(), this->config.battlEye.port);
    bec.sendLogin(this->config.battlEye.password.c_str());
    bec.readResponse(BE_LOGIN);
    if (bec.isLoggedIn()) {
        this->logger->log("BEClient: logged in!");
        bec.sendCommand("#unlock");
        bec.readResponse(BE_COMMAND);
    } else {
        this->logger->log("BEClient: login failed!");
    }
    bec.disconnect();
}

void Epochlib::log(const std::string& log) {
    this->logger->log(log);
}