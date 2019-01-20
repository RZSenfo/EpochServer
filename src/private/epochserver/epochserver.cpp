#include <epochserver/epochserver.hpp>
#include <SteamAPI/SteamAPI.hpp>
#include <BattlEye/BEClient.hpp>
#include <database/DBConfig.hpp>

#include <yaml-cpp/yaml.h>
#include <mariadb++/account.hpp>
#include <mariadb++/connection.hpp>

#include <cstdlib>
#include <ctime>
#include <cctype>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <memory>
#include <filesystem>
#include <random>

bool __iequals(const std::string& string1, const std::string& string2) {
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

EpochServer::EpochServer() {
    
    this->initialized = false;
    
    std::vector<DBConfig> dbconfigs;
    bool error = false;
    try {

        std::filesystem::path configFilePath("@epochserver/config.yaml");
        if (!std::filesystem::exists(configFilePath)) {
            configFilePath = std::filesystem::path("@epochserver/config.yml");
        }
        if (!std::filesystem::exists(configFilePath)) {
            throw std::runtime_error("Configfile not found! Must be @epochserver/config.yml or @epochserver/config.yaml");
        }

        auto stringpath = configFilePath.string();
        YAML::Node config = YAML::LoadFile(configFilePath.string());

        if (!config["connections"].IsMap()) throw std::runtime_error("Connections entry is not a map");

        for (auto& connectionIt : config["connections"]) {
            auto connectionName = connectionIt.first.as<std::string>();
            auto connectionBody = connectionIt.second;

            if (!connectionBody["type"]) throw std::runtime_error("Undefined connection value: \"type\" in " + connectionName);
            if (!connectionBody["ip"]) throw std::runtime_error("Undefined connection value: \"ip\" in " + connectionName);
            if (!connectionBody["username"]) throw std::runtime_error("Undefined connection value: \"username\"" + connectionName);
            if (!connectionBody["password"]) throw std::runtime_error("Undefined connection value: \"password\"" + connectionName);
            if (!connectionBody["database"]) throw std::runtime_error("Undefined connection value: \"database\"" + connectionName);

            DBConfig dbConf;
            dbConf.connectionName = connectionName;
            dbConf.ip = connectionBody["ip"].as<std::string>();
            dbConf.password = connectionBody["password"].as<std::string>();
            dbConf.user = connectionBody["username"].as<std::string>();
            dbConf.port = connectionBody["port"].as<int>(3306);
            dbConf.dbname = connectionBody["database"].as<std::string>();

            auto type = connectionBody["type"].as<std::string>();
            if (__iequals(type, "mysql")) {
                dbConf.dbType = DBType::MY_SQL;
            }
            else if (__iequals(type, "redis")) {
                dbConf.dbType = DBType::REDIS;
            }
            else if (__iequals(type, "sqlite")) {
                dbConf.dbType = DBType::SQLITE;
            }
            else {
                throw std::runtime_error("Unknown database type: \"" + type + "\" in " + connectionName);
            }

            if (connectionBody["statements"].IsMap()) {
                for (auto& statementIt : config["connections"]) {
                    auto statementName = statementIt.first.as<std::string>();
                    auto statementBody = statementIt.second;

                    if (!statementBody["query"]) throw std::runtime_error("Undefined statement value: \"query\" in " + connectionName + "." + statementName);

                    // TODO Statements
                }
            }
            dbconfigs.push_back(dbConf);
        }
    }
    catch (YAML::BadConversion& x) {
        INFO(x.msg);
        error = true;
    }
    catch (YAML::ParserException& x) {
        INFO(x.msg);
        error = true;
    }
    catch (std::runtime_error& x) {
        INFO(x.what);
        error = true;
    }
    

    //Try config load
    if (!error) {
        this->initialized = true;
    }
    else {
        std::exit(1);
    }

    for (auto& config : dbconfigs) {
        if (this->db->init(this->config.db)) {
            WARNING("Database connected: " + config.connectionName);
        }
        else {
            WARNING("Could not initialize Database. Check the config!");
            std::exit(1);
        }
    }
}

EpochServer::~EpochServer() {
    
}

bool EpochServer::callExtensionEntrypoint(char *output, int outputSize, const char *function, const char **args, int argsCnt) {

    
    hiveOutput = "0.6.0.0";
    return false;
}

std::string EpochServer::getConfig() {
    SQF returnSqf;

    returnSqf.push_str(this->config.instanceId.c_str());
    returnSqf.push_number(this->config.steamAPI.key.empty() ? 0 : 1);

    return returnSqf.toArray();
}

std::string EpochServer::initPlayerCheck(int64 _steamId) {
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
                    INFO(log.str());
                }

                if (!proceeded && this->config.steamAPI.vacBanned && document["players"][0]["VACBanned"].GetBool()) {
                    if (this->config.steamAPI.logging >= 1) {
                        std::stringstream log;
                        log << "[SteamAPI] VAC ban " << _steamId << std::endl;
                        log << "- VACBanned: " << (document["players"][0]["VACBanned"].GetBool() ? "true" : "false");
                        INFO(log.str());
                    }

                    this->addBan(_steamId, "VAC Ban");
                    proceeded = true;
                }
                if (!proceeded && this->config.steamAPI.vacMaxDaysSinceLastBan > 0 && document["players"][0]["DaysSinceLastBan"].GetInt() < this->config.steamAPI.vacMaxDaysSinceLastBan) {
                    if (this->config.steamAPI.logging >= 1) {
                        std::stringstream log;
                        log << "[SteamAPI] VAC ban " << _steamId << std::endl;
                        log << "- DaysSinceLastBan: " << document["players"][0]["DaysSinceLastBan"].GetInt();
                        INFO(log.str());
                    }

                    this->addBan(_steamId, "VAC Ban");
                    proceeded = true;
                }
                if (!proceeded && this->config.steamAPI.vacMinNumberOfBans > 0 && document["players"][0]["NumberOfVACBans"].GetInt() >= this->config.steamAPI.vacMinNumberOfBans) {
                    if (this->config.steamAPI.logging >= 1) {
                        std::stringstream log;
                        log << "[SteamAPI] VAC ban " << _steamId << std::endl;
                        log << "- NumberOfVACBans: " << document["players"][0]["NumberOfVACBans"].GetInt();
                        INFO(log.str());
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
                    INFO(log.str());
                }

                if (!proceeded && this->config.steamAPI.playerAllowOlderThan > 0) {
                    std::time_t currentTime = std::time(nullptr);

                    if ((currentTime - document["response"]["players"][0]["timecreated"].GetInt()) < this->config.steamAPI.playerAllowOlderThan) {
                        if (this->config.steamAPI.logging >= 1) {
                            std::stringstream log;
                            log << "[SteamAPI] Player ban " << _steamId << std::endl;
                            log << "- timecreated: " << document["response"]["players"][0]["timecreated"].GetInt() << std::endl;
                            log << "- current: " << currentTime;
                            INFO(log.str());
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

std::string EpochServer::addBan(int64 _steamId, const std::string& _reason) {
    std::string battleyeGUID = this->_getBattlEyeGUID(_steamId);
    std::string bansFilename = this->config.battlEyePath + "/bans.txt";
    SQF returnSQF;

    std::ofstream bansFile(bansFilename, std::ios::app);
    if (bansFile.good()) {
        bansFile << battleyeGUID << " -1 " << _reason << std::endl;
        bansFile.close();

                INFO("BEClient: try to connect " + this->config.battlEye.ip);
                BEClient bec     (this->config.battlEye.ip.c_str(), this->config.battlEye.port);
                bec.sendLogin    (this->config.battlEye.password.c_str());
                bec.readResponse (BE_LOGIN);
                if (bec.isLoggedIn()) {
                    INFO("BEClient: logged in!");
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
                    INFO("BEClient: login failed!");
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

std::string EpochServer::updatePublicVariable(const std::vector<std::string>& _whitelistStrings) {
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
            INFO("publicvariable.txt not found in " + this->config.battlEyePath);
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
    
    INFO("BEClient: try to connect " + this->config.battlEye.ip);
    BEClient bec     (this->config.battlEye.ip.c_str(), this->config.battlEye.port);
    bec.sendLogin    (this->config.battlEye.password.c_str());
    bec.readResponse (BE_LOGIN);
    if (bec.isLoggedIn()) {
        INFO("BEClient: logged in!");
        bec.sendCommand  ("loadEvents");
        bec.readResponse (BE_COMMAND);
    } else {
        INFO("BEClient: login failed!");
    }
    bec.disconnect();

    return returnSQF.toArray();
}
std::string EpochServer::getRandomString(int _stringCount) {
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

std::string EpochServer::getStringMd5(const std::vector<std::string>& _stringsToHash) {
    SQF returnSQF;
    
    for (auto it = _stringsToHash.begin(); it != _stringsToHash.end(); ++it) {
        MD5 md5 = MD5(*it);
        returnSQF.push_str(md5.hexdigest().c_str());
    }

    return returnSQF.toArray();
}

std::string EpochServer::getCurrentTime() {
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

std::string EpochServer::getServerMD5() {

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

std::string EpochServer::_getBattlEyeGUID(int64 _steamId) {
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

bool EpochServer::_loadConfig(const std::string& configFilename) {
    
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

bool EpochServer::_isOffialServer() {
    return this->getServerMD5() == EpochServer_SERVERMD5 ? true : false;
}

// Battleye Integration

void EpochServer::beBroadcastMessage (const std::string& msg) {
    if (msg.empty()) 
        return;

    INFO("BEClient: try to connect " + this->config.battlEye.ip);
    BEClient bec     (this->config.battlEye.ip.c_str(), this->config.battlEye.port);    
    bec.sendLogin    (this->config.battlEye.password.c_str());
    bec.readResponse (BE_LOGIN);
    if (bec.isLoggedIn()) {
        INFO("BEClient: logged in!");
        std::stringstream ss;
        ss << "say -1 " << msg;
        bec.sendCommand  (ss.str().c_str());
        bec.readResponse (BE_COMMAND);
    } else {
        INFO("BEClient: login failed!");
    }
    bec.disconnect();
}

void EpochServer::beKick (const std::string& playerUID, const std::string& msg) {
    if (playerUID.empty() || msg.empty()) 
        return;
    
    INFO("BEClient: try to connect " + this->config.battlEye.ip);
    BEClient bec     (this->config.battlEye.ip.c_str(), this->config.battlEye.port);
    bec.sendLogin    (this->config.battlEye.password.c_str());
    bec.readResponse (BE_LOGIN);
    if (bec.isLoggedIn()) {
        INFO("BEClient: logged in!");
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
            INFO("BEClient: login failed!");
        }
    bec.disconnect();
}

void EpochServer::beBan(const std::string& playerUID, const std::string& msg, const std::string& duration) {
    if (playerUID.empty() || msg.empty())
        return;
            
    INFO("BEClient: try to connect " + this->config.battlEye.ip);
    BEClient bec     (this->config.battlEye.ip.c_str(), this->config.battlEye.port);
    bec.sendLogin    (this->config.battlEye.password.c_str());
    bec.readResponse (BE_LOGIN);
    if (bec.isLoggedIn()) {
        INFO("BEClient: logged in!");
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
        INFO("BEClient: login failed!");
    }
    bec.disconnect   ();
}

void EpochServer::beShutdown() {
    INFO("BEClient: try to connect " + this->config.battlEye.ip);
    BEClient bec     (this->config.battlEye.ip.c_str(), this->config.battlEye.port);
    bec.sendLogin    (this->config.battlEye.password.c_str());
    bec.readResponse (BE_LOGIN);
    if (bec.isLoggedIn()) {
        INFO("BEClient: logged in!");
        bec.sendCommand  ("#shutdown");
        bec.readResponse (BE_COMMAND);
    } else {
        INFO("BEClient: login failed!");
    }    
    bec.disconnect   ();
}

void EpochServer::beLock() {
    INFO("BEClient: try to connect " + this->config.battlEye.ip);
    BEClient bec(this->config.battlEye.ip.c_str(), this->config.battlEye.port);
    bec.sendLogin(this->config.battlEye.password.c_str());
    bec.readResponse(BE_LOGIN);
    if (bec.isLoggedIn()) {
        INFO("BEClient: logged in!");
        bec.sendCommand("#lock");
        bec.readResponse(BE_COMMAND);
    } else {
        INFO("BEClient: login failed!");
    }
    bec.disconnect();
}

void EpochServer::beUnlock() {
    INFO("BEClient: try to connect " + this->config.battlEye.ip);
    BEClient bec(this->config.battlEye.ip.c_str(), this->config.battlEye.port);
    bec.sendLogin(this->config.battlEye.password.c_str());
    bec.readResponse(BE_LOGIN);
    if (bec.isLoggedIn()) {
        INFO("BEClient: logged in!");
        bec.sendCommand("#unlock");
        bec.readResponse(BE_COMMAND);
    } else {
        INFO("BEClient: login failed!");
    }
    bec.disconnect();
}

void EpochServer::log(const std::string& log) {
    INFO(log);
}
