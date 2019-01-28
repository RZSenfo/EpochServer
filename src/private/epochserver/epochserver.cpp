#include <epochserver/epochserver.hpp>
#include <external/md5.hpp>

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


bool __iequals(const std::string& a, const std::string& b) {
    return std::equal(  
        a.begin(), a.end(),
        b.begin(), b.end(),
        [](char a, char b) {
            return tolower(a) == tolower(b);
        }
    );
}

EpochServer::EpochServer() {
    
    bool error = false;
    
    std::filesystem::path configFilePath("@epochserver/config.json");
    if (!std::filesystem::exists(configFilePath)) {
        throw std::runtime_error("Configfile not found! Must be @epochserver/config.json");
    }

    std::ifstream ifs(configFilePath.string());
    rapidjson::IStreamWrapper isw(ifs);
    rapidjson::Document d;
    d.ParseStream<
        rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag,
        rapidjson::UTF8<char>,
        rapidjson::IStreamWrapper
    >(isw);
    
    if (d.HasParseError()) {
        WARNING("Parsing config file failed");
        std::exit(1);
    }
    
    if (!d.HasMember("battleye") || !d["battleye"].IsObject()) {
        WARNING("Battleye entry is not a map");
        std::exit(1);
    }
    try {
        this->__setupBattlEye(d["battleye"]);
    }
    catch (std::runtime_error& x) {
        WARNING(x.what);
        std::exit(1);
    }

    if (d.HasMember("steamapi") && d["steamapi"].IsObject()) {
        try {
            this->__setupSteamApi(d["steamapi"]);
        }
        catch (std::runtime_error& x) {
            WARNING(x.what);
            std::exit(1);
        }
    }
    
    if (!d.HasMember("connections") || !d["connections"].IsObject()) {
       WARNING("Connections entry is not a map");
       std::exit(1);
    }
    try {
        for (rapidjson::Value::ConstMemberIterator itr = d["connections"].MemberBegin(); itr != d["connections"].MemberEnd(); ++itr) {
            this->__setupConnection(
                itr->name.GetString(),
                itr->value
            );
        }
    }
    catch (std::runtime_error& x) {
        WARNING(x.what);
        std::exit(1);
    }
    
}

void EpochServer::__setupBattlEye(const rapidjson::Value& config) {

    if (!config.HasMember("enable")) throw std::runtime_error("Undefined value: \"enable\" in battleye section");
    
    if (!(this->rconEnabled = config["enable"].GetBool())) {
        return;
    }
    
    if (!config.HasMember("ip")) throw std::runtime_error("Undefined value: \"ip\" in battleye section");
    if (!config.HasMember("port")) throw std::runtime_error("Undefined value: \"port\" in battleye section");
    if (!config.HasMember("password")) throw std::runtime_error("Undefined value: \"password\" in battleye section");

    this->rconIp = config["ip"].GetString();
    this->rconPort = config["port"].GetInt();
    this->rconPassword = config["passsword"].GetString();

    this->rcon = std::make_unique<RCON>(
        this->rconIp,
        this->rconPort,
        this->rconPassword,
        true
    );

    if (!config.HasMember("autoreconnect") || config["autoreconnect"].GetBool()) {
        this->rcon->enable_auto_reconnect();
    }

    if (config.HasMember("whitelist") && config["whitelist"].IsObject()) {
        auto whitelistObj = config["whitelist"].GetObject();
        if (whitelistObj.HasMember("enable") && whitelistObj["enable"].GetBool()) {
            this->rcon->set_whitelist_enabled(true);
                
            if (whitelistObj.HasMember("players")) {
                auto list = whitelistObj["players"].GetArray();
                for (auto itr = list.begin(); itr != list.end(); ++itr) {
                    rcon->add_to_whitelist(this->__getBattlEyeGUID(itr->GetUint64()));
                }
            }

            if (whitelistObj.HasMember("openslots")) {
                this->rcon->set_open_slots(whitelistObj["openslots"].GetInt());
            }
            else {
                this->rcon->set_open_slots(0);
            }

            if (whitelistObj.HasMember("maxplayers")) {
                this->rcon->set_max_players(whitelistObj["maxplayers"].GetInt());
            }
            else {
                this->rcon->set_max_players(256);
            }

            if (whitelistObj.HasMember("kickmessage")) {
                this->rcon->set_white_list_kick_message(whitelistObj["kickmessage"].GetString());
            }
            else {
                this->rcon->set_white_list_kick_message("Whitelist Kick!");
            }
        }
    }

    if (config.HasMember("vpndetection") && config["vpndetection"].IsObject()) {
        auto vpnObj = config["vpndetection"].GetObject();
        if (vpnObj.HasMember("enable") && vpnObj["enable"].GetBool()) {
            this->rcon->enable_vpn_detection();

            if (vpnObj.HasMember("exceptions")) {
                auto list = vpnObj["exceptions"].GetArray();
                for (auto itr = list.begin(); itr != list.end(); ++itr) {
                    rcon->add_vpn_detection_guid_exception(this->__getBattlEyeGUID(itr->GetUint64()));
                }
            }

            if (vpnObj.HasMember("iphubapikey")) {
                this->rcon->set_iphub_api_key(vpnObj["iphubapikey"].GetString());
            }

            if (vpnObj.HasMember("kickmessage")) {
                this->rcon->set_vpn_detect_kick_msg(vpnObj["kickmessage"].GetString());
            }
            else {
                this->rcon->set_vpn_detect_kick_msg("Whitelist Kick!");
            }

            if (vpnObj.HasMember("kicksuspecious") && vpnObj["kicksuspecious"].GetBool()) {
                this->rcon->enable_vpn_suspecious_kicks();
            }
        }
    }

    if (config.HasMember("tasks") && config["tasks"].IsArray()) {
        auto tasklist = config["tasks"].GetArray();
        for (auto itr = tasklist.begin(); itr != tasklist.end(); ++itr) {

            bool enable = (*itr)["enable"].GetBool();
            if (!enable) continue;

            std::string type = (*itr)["type"].GetString();
            std::string data = (*itr)["data"].GetString();
            bool repeat = (*itr)["repeat"].GetBool();
            int seconds = (*itr)["delay"].GetInt();
            int initdelay = (*itr)["initialdelay"].GetInt();

            if (type == "GLOBALCHAT") {
                this->rcon->add_task(RconTaskType::GLOBALMESSAGE, data, repeat, seconds, initdelay);
            }
            else if (type == "KICKALL") {
                this->rcon->add_task(RconTaskType::KICKALL, data, repeat, seconds, initdelay);
            }
            else if (type == "SHUTDOWN") {
                this->rcon->add_task(RconTaskType::SHUTDOWN, data, repeat, seconds, initdelay);
            }
            else if (type == "LOCK") {
                this->rcon->add_task(RconTaskType::SHUTDOWN, data, repeat, seconds, initdelay);
            }
            else if (type == "UNLOCK") {
                this->rcon->add_task(RconTaskType::SHUTDOWN, data, repeat, seconds, initdelay);
            }
        }
    }

    rcon->start();
}

void EpochServer::__setupSteamApi(const rapidjson::Value& config) {
    if (!config.HasMember("enable")) throw std::runtime_error("Undefined value: \"enable\" in steamapi section");
    
    if (!config["enable"].GetBool()) {
        return;
    }
    if (!config.HasMember("apikey")) throw std::runtime_error("Undefined value: \"apikey\" in steamapi section");
    std::string apikey = config["apikey"].GetString();
    if (apikey.empty()) {
        throw std::runtime_error("Steam API key must be set if you want to use this feature");
    }

    this->steamApi = std::make_unique<SteamAPI>(apikey);

    this->steamApiLogLevel = config.HasMember("loglevel") ? config["loglevel"].GetInt() : 0;
    this->minDaysSinceLastVacBan = config.HasMember("mindayssincelastban") ? config["mindayssincelastban"].GetInt() : -1;
    this->kickVacBanned = config.HasMember("kickvacbanned") ? config["kickvacbanned"].GetBool() : false;
    this->minAccountAge = config.HasMember("minaccountage") ? config["minaccountage"].GetInt() : -1;
}

void EpochServer::__setupConnection(const std::string& name, const rapidjson::Value& config) {
    
    if (config.HasMember("enable") && !config["enable"].GetBool()) {
        return;
    }
    
    DBConfig dbConf;
    
    if (!config.HasMember("type")) throw std::runtime_error("Undefined connection value: \"type\" in " + name);
    std::string type = config["type"].GetString();
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
        throw std::runtime_error("Unknown database type: \"" + type + "\" in " + name);
    }
    
    if (!config.HasMember("ip")) throw std::runtime_error("Undefined connection value: \"ip\" in " + name);
    if (!config.HasMember("username")) throw std::runtime_error("Undefined connection value: \"username\" in " + name);
    if (!config.HasMember("password")) throw std::runtime_error("Undefined connection value: \"password\" in " + name);
    if (!config.HasMember("database")) throw std::runtime_error("Undefined connection value: \"database\" in " + name);

    dbConf.connectionName = name;
    dbConf.ip = config["ip"].GetString();
    dbConf.password = config["password"].GetString();
    dbConf.user = config["username"].GetString();
    dbConf.port = config["port"].GetInt();
    dbConf.dbname = config["database"].GetString();

    if (config.HasMember("statements") && config["statements"].IsObject()) {
        for (auto itr = config["statements"].MemberBegin(); itr != config["statements"].MemberEnd(); ++itr) {
            std::string statementName = itr->name.GetString();
            auto statementBody = itr->value.GetObject();

            if (!statementBody.HasMember("query")) throw std::runtime_error("Undefined statement value: \"query\" in " + name + "." + statementName);
            if (!statementBody.HasMember("params")) throw std::runtime_error("Undefined statement value: \"params\" in " + name + "." + statementName);
            if (!statementBody.HasMember("result")) throw std::runtime_error("Undefined statement value: \"result\" in " + name + "." + statementName);

            DBSQLStatementTemplate statement;
            statement.statementName = statementName;
            statement.query = statementBody["query"].GetString();
            statement.isInsert = statementBody.HasMember("isinsert") && statementBody["isinsert"].GetBool();

            auto paramslist = statementBody["params"].GetArray();
            statement.params.reserve(paramslist.Size());
            for (auto itr = paramslist.begin(); itr != paramslist.end(); ++itr) {
                auto typeStr = itr->GetString();
                if (typeStr == "number") {
                    statement.params.emplace_back(DBSQLStatementParamType::NUMBER);
                }
                else if (typeStr == "array") {
                    statement.params.emplace_back(DBSQLStatementParamType::ARRAY);
                }
                else if (typeStr == "bool") {
                    statement.params.emplace_back(DBSQLStatementParamType::BOOL);
                }
                else if (typeStr == "string") {
                    statement.params.emplace_back(DBSQLStatementParamType::STRING);
                }
                else {
                    throw std::runtime_error("Unknown type in params types of " + statementName);
                }
            }

            auto resultslist = statementBody["results"].GetArray();
            statement.result.reserve(resultslist.Size());
            for (auto itr = resultslist.begin(); itr != resultslist.end(); ++itr) {
                auto typeStr = itr->GetString();
                if (typeStr == "number") {
                    statement.result.emplace_back(DBSQLStatementParamType::NUMBER);
                }
                else if (typeStr == "array") {
                    statement.result.emplace_back(DBSQLStatementParamType::ARRAY);
                }
                else if (typeStr == "bool") {
                    statement.result.emplace_back(DBSQLStatementParamType::BOOL);
                }
                else if (typeStr == "string") {
                    statement.result.emplace_back(DBSQLStatementParamType::STRING);
                }
                else {
                    throw std::runtime_error("Unknown type in result types of " + statementName);
                }
            }

            dbConf.statements.emplace_back(statement);
        }
    }
    
}

EpochServer::~EpochServer() {
    
}

int EpochServer::callExtensionEntrypoint(char *output, int outputSize, const char *function, const char **args, int argsCnt) {
    
    // TODO
    strncpy_s(output, outputSize, "0.1.0.0", _TRUNCATE);
    return 0;
}

bool EpochServer::initPlayerCheck(uint64 _steamId) {

    // already looked up
    if (std::find(this->checkedSteamIds.begin(), this->checkedSteamIds.end(), _steamId) != this->checkedSteamIds.end()) {
        return true;
    }
    bool kick = false;
    auto steamIdStr = std::to_string(_steamId);

    // SteamAPI is set up
    if (this->steamApi) {
        
        auto bans = this->steamApi->GetPlayerBans(steamIdStr);

        // VAC check
        if (bans.NumberOfGameBans > 0) {
            if (this->steamApiLogLevel >= 2) {
                std::stringstream log;
                log << "[SteamAPI] VAC check " << _steamId << std::endl;
                log << "- VACBanned: " << (bans.VACBanned ? "true" : "false") << std::endl;
                log << "- NumberOfVACBans: " << bans.NumberOfGameBans;
                INFO(log.str());
            }

            if (!kick && this->kickVacBanned && bans.VACBanned) {
                if (this->steamApiLogLevel >= 1) {
                    std::stringstream log;
                    log << "[SteamAPI] VAC ban " << _steamId << std::endl;
                    log << "- VACBanned: " << (bans.VACBanned ? "true" : "false");
                    INFO(log.str());
                }

                this->addBan(_steamId, "VAC Ban");
                kick = true;
            }
            if (!kick && this->minDaysSinceLastVacBan > 0 && bans.DaysSinceLastBan < this->minDaysSinceLastVacBan) {
                if (this->steamApiLogLevel >= 1) {
                    std::stringstream log;
                    log << "[SteamAPI] VAC ban " << _steamId << std::endl;
                    log << "- DaysSinceLastBan: " << bans.DaysSinceLastBan;
                    INFO(log.str());
                }

                this->addBan(_steamId, "VAC Ban");
                kick = true;
            }
            if (!kick && this->maxVacBans > 0 && bans.NumberOfGameBans >= this->maxVacBans) {
                if (this->steamApiLogLevel >= 1) {
                    std::stringstream log;
                    log << "[SteamAPI] VAC ban " << _steamId << std::endl;
                    log << "- NumberOfVACBans: " << bans.NumberOfGameBans;
                    INFO(log.str());
                }

                this->addBan(_steamId, "VAC Ban");
                kick = true;
            }
        }

        // Player check
        auto summary = this->steamApi->GetPlayerSummaries(steamIdStr);
        if (!kick) {
            if (this->steamApiLogLevel >= 2) {
                std::stringstream log;
                log << "[SteamAPI] Player check " << _steamId << std::endl;
                log << "- timecreated: " << summary.timecreated;
                INFO(log.str());
            }

            if (!kick && this->minAccountAge > 0 && summary.timecreated > -1) {
                std::time_t currentTime = std::time(0);
                int age = currentTime - summary.timecreated;
                age /= (24 * 60 * 60);

                if (age > 0 && age < this->minAccountAge) {
                    if (this->steamApiLogLevel >= 1) {
                        std::stringstream log;
                        log << "[SteamAPI] Player ban " << _steamId << std::endl;
                        log << "- timecreated: " << summary.timecreated << std::endl;
                        log << "- current: " << currentTime;
                        INFO(log.str());
                    }

                    this->addBan(_steamId, "New account filter");
                    kick = true;
                }
            }
        }

        // Not kick -> fine
        if (!kick) {
            this->checkedSteamIdsMutex.lock();
            this->checkedSteamIds.push_back(_steamId);
            this->checkedSteamIdsMutex.unlock();
        }
        else {
            this->beKick(steamIdStr);
        }
    }

    return !kick;
}

void EpochServer::addBan(uint64 _steamId, const std::string& _reason) {
    std::string battleyeGUID = this->__getBattlEyeGUID(_steamId);
    this->rcon->add_ban(battleyeGUID, _reason);
}

std::string EpochServer::updatePublicVariable(const std::vector<std::string>& _whitelistStrings) {
    /* TODO
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
    */

    return "";
}

std::string EpochServer::getRandomString() {
    
    // Define char pool
    const char charPool[] = "abcdefghijklmnopqrstuvwxyz";
    int charPoolSize = sizeof(charPool) - 1;

    std::string randomString;
    int stringLength = (std::rand() % 5) + 5; //random string size between 5-10

    for (int i = 0; i < stringLength; i++) {
        randomString += charPool[std::rand() % charPoolSize];
    }

    return randomString;
}

std::vector<std::string> EpochServer::getStringMd5(const std::vector<std::string>& _stringsToHash) {
    
    std::vector<std::string> out;
    out.reserve(_stringsToHash.size());

    for (auto it = _stringsToHash.begin(); it != _stringsToHash.end(); ++it) {
        MD5 md5 = MD5(*it);
        out.emplace_back(md5.hexdigest());
    }

    return out;
}

std::string EpochServer::getCurrentTime() {
    
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "[%Y,%m,%d,%H,%M,%S]", &tstruct);

    return buf;
}

std::string EpochServer::__getBattlEyeGUID(uint64 _steamId) {
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

    return MD5(bestring.str()).hexdigest();
}

// Battleye Integration

void EpochServer::beBroadcastMessage(const std::string& msg) {
    if (msg.empty() || !this->rcon) {
        return;
    }
    this->rcon->send_global_msg(msg);
}

void EpochServer::beKick(const std::string& playerGUID) {
    if (playerGUID.empty() || !this->rcon) {
        return;
    }
    this->rcon->kick(playerGUID);
}

void EpochServer::beBan(const std::string& playerUID, const std::string& msg, int duration) {
    if (playerUID.empty() || msg.empty() || !this->rcon) {
        return;
    }
     
    this->rcon->add_ban(playerUID, msg, duration);
}

void EpochServer::beShutdown() {
    
    // TODO maybe differently?
    std::exit(0);
}

void EpochServer::beLock() {
    if (!this->rcon) {
        return;
    }
    this->rcon->send_command("#lock");
}

void EpochServer::beUnlock() {
    if (!this->rcon) {
        return;
    }
    this->rcon->send_command("#unlock");
}

void EpochServer::log(const std::string& log) {
    INFO(log);
}
