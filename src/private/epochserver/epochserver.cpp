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
        WARNING(x.what());
        std::exit(1);
    }

    if (d.HasMember("steamapi") && d["steamapi"].IsObject()) {
        try {
            this->__setupSteamApi(d["steamapi"]);
        }
        catch (std::runtime_error& x) {
            WARNING(x.what());
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
        WARNING(x.what());
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
    
    
    if (utils::iequals(type, "mysql")) {

        if (!config.HasMember("database")) throw std::runtime_error("Undefined connection value: \"database\" in " + name);
        
        dbConf.dbType = DBType::MY_SQL;
        dbConf.ip = config.HasMember("ip") ? config["ip"].GetString() : "127.0.0.1";
        dbConf.password = config.HasMember("password") ? config["password"].GetString() : "";
        dbConf.user = config.HasMember("username") ? config["username"].GetString() : "root";;
        dbConf.port = config.HasMember("port") ? config["port"].GetInt() : 3306;
        dbConf.dbname = config["database"].GetString();
    }
    else if (utils::iequals(type, "redis")) {

        if (!config.HasMember("password")) throw std::runtime_error("Undefined connection value: \"password\" in " + name);
        if (!config.HasMember("database")) throw std::runtime_error("Undefined connection value: \"database\" in " + name);

        dbConf.dbType = DBType::REDIS;
        dbConf.ip = config.HasMember("ip") ? config["ip"].GetString() : "127.0.0.1";
        dbConf.port = config.HasMember("port") ? config["port"].GetInt() : 6379;
        dbConf.password = config.HasMember("password") ? config["password"].GetString() : "";
    }
    else if (utils::iequals(type, "sqlite")) {
        
        if (!config.HasMember("database")) throw std::runtime_error("Undefined connection value: \"database\" in " + name);

        dbConf.dbType = DBType::SQLITE;
        dbConf.dbname = config["database"].GetString();
    }
    else {
        throw std::runtime_error("Unknown database type: \"" + type + "\" in " + name);
    }
    
    dbConf.connectionName = name;
    
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
                    statement.params.emplace_back(DBSQLStatementParamType::DB_NUMBER);
                }
                else if (typeStr == "array") {
                    statement.params.emplace_back(DBSQLStatementParamType::DB_ARRAY);
                }
                else if (typeStr == "bool") {
                    statement.params.emplace_back(DBSQLStatementParamType::DB_BOOL);
                }
                else if (typeStr == "string") {
                    statement.params.emplace_back(DBSQLStatementParamType::DB_STRING);
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
                    statement.result.emplace_back(DBSQLStatementParamType::DB_NUMBER);
                }
                else if (typeStr == "array") {
                    statement.result.emplace_back(DBSQLStatementParamType::DB_ARRAY);
                }
                else if (typeStr == "bool") {
                    statement.result.emplace_back(DBSQLStatementParamType::DB_BOOL);
                }
                else if (typeStr == "string") {
                    statement.result.emplace_back(DBSQLStatementParamType::DB_STRING);
                }
                else {
                    throw std::runtime_error("Unknown type in result types of " + statementName);
                }
            }

            dbConf.statements.emplace_back(statement);
        }
    }

    this->dbWorkers.emplace_back(
        std::pair< std::string, std::shared_ptr<DBWorker> >( name, std::make_shared<DBWorker>(dbConf) )
    );
    
}

EpochServer::~EpochServer() {
    
}

bool EpochServer::initPlayerCheck(const std::string& steamIdStr) {

    auto _steamId = std::stoull(steamIdStr);
    {
        std::shared_lock<std::shared_mutex> lock(this->checkedSteamIdsMutex);
        // already looked up
        if (std::find(this->checkedSteamIds.begin(), this->checkedSteamIds.end(), _steamId) != this->checkedSteamIds.end()) {
            return true;
        }
    }
    bool kick = false;

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

                this->beBan(this->__getBattlEyeGUID(_steamId), "VAC Ban", -1);
                kick = true;
            }
            if (!kick && this->minDaysSinceLastVacBan > 0 && bans.DaysSinceLastBan < this->minDaysSinceLastVacBan) {
                if (this->steamApiLogLevel >= 1) {
                    std::stringstream log;
                    log << "[SteamAPI] VAC ban " << _steamId << std::endl;
                    log << "- DaysSinceLastBan: " << bans.DaysSinceLastBan;
                    INFO(log.str());
                }

                this->beBan(this->__getBattlEyeGUID(_steamId), "VAC Ban", -1);
                kick = true;
            }
            if (!kick && this->maxVacBans > 0 && bans.NumberOfGameBans >= this->maxVacBans) {
                if (this->steamApiLogLevel >= 1) {
                    std::stringstream log;
                    log << "[SteamAPI] VAC ban " << _steamId << std::endl;
                    log << "- NumberOfVACBans: " << bans.NumberOfGameBans;
                    INFO(log.str());
                }

                this->beBan(this->__getBattlEyeGUID(_steamId), "VAC Ban", -1);
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

                    this->beBan(this->__getBattlEyeGUID(_steamId), "Account not old enough", -1);
                    kick = true;
                }
            }
        }

        // Not kick -> fine
        if (!kick) {
            std::unique_lock<std::shared_mutex> lock(this->checkedSteamIdsMutex);
            this->checkedSteamIds.push_back(_steamId);
        }
        else {
            this->beKick(steamIdStr);
        }
    }

    return !kick;
}

std::string EpochServer::getRandomString() {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_int_distribution<> dis('a', 'z');

    unsigned short strLen = 10;
    std::string str;
    str.reserve(strLen);
    for (int i = 0; i < strLen; i++) {
        str += static_cast<char>(dis(gen));
    }
    return str;
}

std::string EpochServer::getStringMd5(const std::string& stringToHash) {
    MD5 md5 = MD5(stringToHash);
    return md5.hexdigest();
}

std::vector<std::string> EpochServer::getStringMd5(const std::vector<std::string>& _stringsToHash) {
    
    std::vector<std::string> out;
    out.reserve(_stringsToHash.size());

    for (auto it = _stringsToHash.begin(); it != _stringsToHash.end(); ++it) {
        out.emplace_back(this->getStringMd5(*it));
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

// Battleye / RCON Integration

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

std::shared_ptr<DBWorker> EpochServer::__getDbWorker(const std::string& name) {
    for (auto& x : this->dbWorkers) {
        if (x.first == name) {
            return x.second;
        }
    }
    throw std::runtime_error("No worker found for name: " + name);
}

#define SET_RESULT(x,y) outCode = x; out = y;

// TODO test if moving args works
int EpochServer::callExtensionEntrypoint(char *output, int outputSize, const char *function, const char **args, int argsCnt) {

    std::string out;
    int outCode = 0;

    size_t fncLen = strlen(function);

    try {
        if (fncLen == 3) {
            /* TODO Function codes
            switch (function[0]) {
            // db
            case '1': {
                bool async = function[2] == '1';
                switch (function[1]) {
                    // Poll
                    case '0': {
                        break;
                    }
                    // get
                    case '1': {
                        if (argsCnt < 2) throw std::runtime_error("Not enough args given for get");
                        auto &worker = this->__getDbWorker(args[0]);
                        unsigned long id = worker->get<DBExecutionType::ASYNC_POLL>(args[1]);
                        SET_RESULT(0, std::to_string(id));
                        break;
                    }
                    // getTtl
                    case '2': {
                        break;
                    }
                    // set
                    case '3': {
                        break;
                    }
                    // setEx
                    case '4': {
                        break;
                    }
                    // query
                    case '5': {
                        break;
                    }
                    // Exists
                    case '6': {
                        break;
                    }
                    // Expire
                    case '7': {
                        break;
                    }
                    // del
                    case '8': {
                        break;
                    }
                    // getRange
                    case '9': {
                        break;
                    }
                default: { SET_RESULT(1, "Unknown function"); }
                };
                break;
            }
            //be
            case '2': {
                break;
            }
            // 3-9 TODO
            // util
            case '0': {
                break;
            }
            default: { SET_RESULT(1, "Unknown function"); }
            };
            */
        }
        else if (!strncmp(function, "db", 2)) {
            // skip prefix TODO test encoding.. might need to skipp more bytes
            function += 2;

            if (!strcmp(function, "Poll")) {
                if (argsCnt < 2 || strlen(args[0]) == 0 || strlen(args[1]) == 0) {
                    throw std::runtime_error("Invalid args for dbPoll");
                }

                auto &worker = this->__getDbWorker(args[0]);

                if (!worker) throw std::runtime_error(static_cast<std::string>("Database connection not found: ") + args[0]);

                unsigned long id = std::stoul(args[1]);

                try {
                    auto res = worker->tryPopResult(id);
                    if (res.index() == 0) {
                        SET_RESULT(0, std::get<std::string>(res));
                    }
                    else if (res.index() == 1) {
                        SET_RESULT(0, std::to_string(std::get<bool>(res)));
                    }
                    else {
                        SET_RESULT(0, std::to_string(std::get<int>(res)));
                    }
                }
                catch (std::invalid_argument& e) {
                    outCode = 2;
                    throw std::runtime_error("Request not ready");
                }
                catch (std::out_of_range& e) {
                    throw std::runtime_error("Request id unknown");
                }
                catch (std::exception& ex) {
                    outCode = 3;
                    throw std::runtime_error(std::string("Error occured for request: ") + ex.what());
                }
            }
            else if (!strcmp(function, "Get")) {
                if (argsCnt < 2) throw std::runtime_error("Not enough args given for get");
                auto &worker = this->__getDbWorker(args[0]);

                unsigned long id = worker->get<DBExecutionType::ASYNC_POLL>(args[1]);
                SET_RESULT(0, std::to_string(id));

            }
            else if (!strcmp(function, "Exists")) {
                if (argsCnt < 2) throw std::runtime_error("Not enough args given for exists");
                auto &worker = this->__getDbWorker(args[0]);

                unsigned long id = worker->exists<DBExecutionType::ASYNC_POLL>(args[1]);
                SET_RESULT(0, std::to_string(id));
            }
            else if (!strcmp(function, "Set")) {
                if (argsCnt < 3) throw std::runtime_error("Not enough args given for set");
                auto &worker = this->__getDbWorker(args[0]);
                worker->set<DBExecutionType::ASYNC_CALLBACK>(args[1], args[2], std::nullopt, std::nullopt);
            }
            else if (!strcmp(function, "SetEx")) {
                if (argsCnt < 4) throw std::runtime_error("Not enough args given for setEx");
                auto &worker = this->__getDbWorker(args[0]);
                int ttl = std::stoi(args[2]);
                worker->setEx<DBExecutionType::ASYNC_CALLBACK>(args[1], ttl, args[3], std::nullopt, std::nullopt);
            }
            else if (!strcmp(function, "Expire")) {
                if (argsCnt < 2) throw std::runtime_error("Not enough args given for expire");
                auto &worker = this->__getDbWorker(args[0]);
                int ttl = std::stoi(args[2]);
                worker->expire<DBExecutionType::ASYNC_CALLBACK>(args[1], ttl, std::nullopt, std::nullopt);
            }
            else if (!strcmp(function, "Del")) {
                if (argsCnt < 2) throw std::runtime_error("Not enough args given for del");
                auto &worker = this->__getDbWorker(args[0]);
                worker->del<DBExecutionType::ASYNC_CALLBACK>(args[1], std::nullopt, std::nullopt);
            }
            else if (!strcmp(function, "Query")) {
                // TODO
                SET_RESULT(1, "TODO");
            }
            else if (!strcmp(function, "GetTtl")) {
                if (argsCnt < 2) throw std::runtime_error("Not enough args given for getTtl");
                auto &worker = this->__getDbWorker(args[0]);

                unsigned long id = worker->getWithTtl<DBExecutionType::ASYNC_POLL>(args[1]);
                SET_RESULT(0, std::to_string(id));
            }
            else if (!strcmp(function, "SetSync")) {
                if (argsCnt < 3) throw std::runtime_error("Not enough args given for set");
                auto &worker = this->__getDbWorker(args[0]);

                unsigned long id = worker->set<DBExecutionType::ASYNC_POLL>(args[1], args[2]);
                SET_RESULT(0, std::to_string(id));
            }
            else if (!strcmp(function, "SetExSync")) {
                if (argsCnt < 4) throw std::runtime_error("Not enough args given for setEx");
                auto &worker = this->__getDbWorker(args[0]);
                int ttl = std::stoi(args[2]);
                unsigned long id = worker->setEx<DBExecutionType::ASYNC_POLL>(args[1], ttl, args[3]);
                SET_RESULT(0, std::to_string(id));
            }
            else if (!strcmp(function, "ExpireSync")) {
                if (argsCnt < 2) throw std::runtime_error("Not enough args given for expire");
                auto &worker = this->__getDbWorker(args[0]);

                int ttl = std::stoi(args[2]);
                unsigned long id = worker->expire<DBExecutionType::ASYNC_POLL>(args[1], ttl);
                SET_RESULT(0, std::to_string(id));
            }
            else if (!strcmp(function, "DelSync")) {
                if (argsCnt < 2) throw std::runtime_error("Not enough args given for del");
                auto &worker = this->__getDbWorker(args[0]);

                unsigned long id = worker->del<DBExecutionType::ASYNC_POLL>(args[1]);
                SET_RESULT(0, std::to_string(id));
            }
            else if (!strcmp(function, "GetRange")) {
                if (argsCnt < 4) throw std::runtime_error("Not enough args given for getRange");
                auto &worker = this->__getDbWorker(args[0]);

                unsigned long from, to;
                from = std::stoul(args[2]);
                to = std::stoul(args[3]);
                unsigned long id = worker->getRange<DBExecutionType::ASYNC_POLL>(args[1], from, to);
                SET_RESULT(0, std::to_string(id));
            }
            else if (!strcmp(function, "Ping")) {
                auto &worker = this->__getDbWorker(args[0]);

                unsigned long id = worker->ping<DBExecutionType::ASYNC_POLL>();
                SET_RESULT(0, std::to_string(id));
            }
            else {
                SET_RESULT(1, "Unknown db command");
            }
        }
        else if (!strncmp(function, "be", 2)) {
            if (!strcmp(function, "beBroadcastMessage")) {
                //(const std::string& message);
                if (argsCnt < 1) throw std::runtime_error("Missing message param for beBroadcastMessage");
                threadpool->fireAndForget([this, x = std::string(args[0])]() {
                    this->beBroadcastMessage(x);
                });
            }
            else if (!strcmp(function, "beKick")) {
                //(const std::string& playerGUID);
                if (argsCnt < 1) throw std::runtime_error("Missing guid param in beKick");
                threadpool->fireAndForget([this, x = std::string(args[0])]() {
                    this->beKick(x);
                });
            }
            else if (!strcmp(function, "beBan")) {
                // (const std::string& playerGUID, const std::string& message, int duration);
                if (argsCnt < 1) throw std::runtime_error("Missing params for beBan");
                std::string msg = argsCnt > 1 ? args[1] : "BE Ban";
                if (msg.empty()) msg = "BE Ban";
                int dur = -1;
                if (argsCnt > 2) {
                    try {
                        dur = std::stoi(args[2]);
                    }
                    catch (...) {
                        WARNING(static_cast<std::string>("Could not parse banDuration, fallback to permaban. GUID: ") + args[0]);
                    }
                }
                threadpool->fireAndForget([this, uid = std::string(args[0]), msg = std::move(msg), dur]() {
                    this->beBan(uid, msg, dur);
                });
            }
            else if (!strcmp(function, "beShutdown")) {
                this->beShutdown();
            }
            else if (!strcmp(function, "beLock")) {
                threadpool->fireAndForget([this]() {
                    this->beLock();
                });
            }
            else if (!strcmp(function, "beUnlock")) {
                threadpool->fireAndForget([this]() {
                    this->beUnlock();
                });
            }
            else {
                SET_RESULT(1, "Unknown be command");
            }
        }
        else if (!strcmp(function, "extLog")) {
            if (argsCnt < 1) throw std::runtime_error("Nothing to log was provided");
            
            std::string msg = args[0];
            for (int i = 1; i < argsCnt; ++i) {
                msg += "\n";
                msg += args[i];
            }

            threadpool->fireAndForget([x = std::move(msg)]() {
                INFO(x);
            });
        }
        else if (!strcmp(function, "playerCheck")) {
            if (argsCnt < 1) throw std::runtime_error("No playerid given to check");
            if (strlen(args[0]) != 17) throw std::runtime_error("Playerid is not a steam64id. Length mismatch");
            threadpool->fireAndForget([this, x = std::move(std::string(args[0]))]() {
                this->initPlayerCheck(x);
            });
        }
        else if (!strcmp(function, "getCurrentTime")) {
            SET_RESULT(0, this->getCurrentTime());
        }
        else if (!strcmp(function, "getRandomString")) {
            SET_RESULT(0, this->getRandomString());
        }
        else if (!strcmp(function, "getStringMd5")) {
            // string
            if (argsCnt < 1) throw std::runtime_error("No string to hash was given");
            SET_RESULT(0, this->getStringMd5(args[0]));
        }
        else if (!strcmp(function, "version")) {
            SET_RESULT(1, "0.1.0.0");
        }
        else {
            SET_RESULT(1, "Unknown function");
        }
    
    }
    catch (std::exception& e) {
        outCode = outCode ? outCode : 1;
        out = static_cast<std::string>("Error occured: ") + e.what();
    }

    strncpy_s(output, outputSize, out.c_str(), _TRUNCATE);
    return outCode;
}
