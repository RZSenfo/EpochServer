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
    
    auto cur_path = std::filesystem::path();
    INFO( std::string("Looking for Server config file.. (Base: ") + cur_path.generic_string() + ")" );

    std::filesystem::path configFilePath("@epochserver/config.json");
    if (!std::filesystem::exists(configFilePath)) {
        WARNING("Configfile not found! Must be @epochserver/config.json");
        std::exit(1);
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
        this->__setupRCON(d["battleye"]);
    }
    catch (std::runtime_error& x) {
        WARNING(x.what());
        std::exit(1);
    }

    if (d.HasMember("steamapi") && d["steamapi"].IsObject()) {
        try {
            this->__setupSteamAPI(d["steamapi"]);
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
        this->dbManager = std::make_unique<DBManager>(d["connections"]);
    }
    catch (std::runtime_error& x) {
        WARNING(x.what());
        std::exit(1);
    }
    
}

void EpochServer::__setupRCON(const rapidjson::Value& config) {

    if (!config.HasMember("enable")) throw std::runtime_error("Undefined value: \"enable\" in battleye section");
    
    if (!config["enable"].GetBool()) {
        return;
    }
    
    if (!config.HasMember("ip")) throw std::runtime_error("Undefined value: \"ip\" in battleye section");
    if (!config.HasMember("port")) throw std::runtime_error("Undefined value: \"port\" in battleye section");
    if (!config.HasMember("password")) throw std::runtime_error("Undefined value: \"password\" in battleye section");

    auto rconIp = config["ip"].GetString();
    auto rconPort = config["port"].GetInt();
    auto rconPassword = config["passsword"].GetString();

    this->rcon = std::make_shared<RCON>(rconIp, rconPort, rconPassword);

    if (!config.HasMember("autoreconnect") || config["autoreconnect"].GetBool()) {
        this->rcon->enable_auto_reconnect();
    }

    rcon->start();


    if (config.HasMember("whitelist") && config["whitelist"].IsObject()) {
        
        
        auto whitelistObj = config["whitelist"].GetObject();
        if (whitelistObj.HasMember("enable") && whitelistObj["enable"].GetBool()) {
        
            this->whitelist = std::make_shared<Whitelist>(this->rcon);
                
            if (whitelistObj.HasMember("players")) {
                auto list = whitelistObj["players"].GetArray();
                for (auto itr = list.begin(); itr != list.end(); ++itr) {
                    this->whitelist->add_to_whitelist(this->__getBattlEyeGUID(itr->GetUint64()));
                }
            }

            if (whitelistObj.HasMember("openslots")) {
                this->whitelist->set_open_slots(whitelistObj["openslots"].GetInt());
            }

            if (whitelistObj.HasMember("maxplayers")) {
                this->whitelist->set_max_players(whitelistObj["maxplayers"].GetInt());
            }
            else {
                this->whitelist->set_max_players(1024); // imagine that
            }

            if (whitelistObj.HasMember("kickmessage")) {
                this->whitelist->set_white_list_kick_message(whitelistObj["kickmessage"].GetString());
            }
            else {
                this->whitelist->set_white_list_kick_message("Whitelist Kick!");
            }
            this->whitelist->set_whitelist_enabled(true);

            if (config.HasMember("vpndetection") && config["vpndetection"].IsObject()) {
                auto vpnObj = config["vpndetection"].GetObject();
                if (vpnObj.HasMember("enable") && vpnObj["enable"].GetBool() && vpnObj.HasMember("iphubapikey")) {
                    
                    std::string apikey = vpnObj["iphubapikey"].GetString();
                    if (apikey.empty()) {
                        return;
                    }

                    this->whitelist->enable_vpn_detection(
                        /* api key */ apikey,
                        /* kicksuspecious */ vpnObj.HasMember("kicksuspecious") && vpnObj["kicksuspecious"].GetBool()
                    );

                    if (vpnObj.HasMember("exceptions")) {
                        auto list = vpnObj["exceptions"].GetArray();
                        for (auto itr = list.begin(); itr != list.end(); ++itr) {
                            this->whitelist->add_vpn_detection_guid_exception(this->__getBattlEyeGUID(itr->GetUint64()));
                        }
                    }

                    if (vpnObj.HasMember("kickmessage")) {
                        this->whitelist->set_vpn_detect_kick_msg(vpnObj["kickmessage"].GetString());
                    }
                    else {
                        this->whitelist->set_vpn_detect_kick_msg("Whitelist Kick!");
                    }
                }
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

}

void EpochServer::__setupSteamAPI(const rapidjson::Value& config) {
    if (!config.HasMember("enable")) throw std::runtime_error("Undefined value: \"enable\" in steamapi section");
    
    if (!config["enable"].GetBool()) {
        return;
    }
    if (!config.HasMember("apikey")) throw std::runtime_error("Undefined value: \"apikey\" in steamapi section");
    std::string apikey = config["apikey"].GetString();
    if (apikey.empty()) {
        throw std::runtime_error("Steam API key must be set if you want to use this feature");
    }
    this->steamApi = std::make_shared<SteamAPI>(apikey);

    this->steamApi->setSteamApiLogLevel(config.HasMember("loglevel") ? config["loglevel"].GetInt() : 0);
    this->steamApi->setMinDaysSinceLastVacBan(config.HasMember("mindayssincelastban") ? config["mindayssincelastban"].GetInt() : -1);
    this->steamApi->setKickVacBanned(config.HasMember("kickvacbanned") ? config["kickvacbanned"].GetBool() : false);
    this->steamApi->setMinAccountAge(config.HasMember("minaccountage") ? config["minaccountage"].GetInt() : -1);
    this->steamApi->setMaxVacBans(config.HasMember("maxvacbans") ? config["maxvacbans"].GetInt() : -1);

}

EpochServer::~EpochServer() {}

void EpochServer::insertCallback(const SQFCallBackHandle& cb) {

    std::string x;
    cb.toString(x);
    std::pair<std::string, size_t> pair{ x, 0 };

    std::unique_lock<std::mutex> lock(this->resultsMutex);
    this->results.emplace(pair);
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

std::string EpochServer::getUniqueId() {
    
    auto str = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()); // 20 chars long
    str.reserve(24);
    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_int_distribution<> dis('0', '9');
    str += static_cast<char>(dis(gen));
    str += static_cast<char>(dis(gen));
    str += static_cast<char>(dis(gen));
    str += static_cast<char>(dis(gen));

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
    std::stringstream buffer;
    auto t = std::time(nullptr);
    auto tm = std::localtime(&t);
    buffer << std::put_time(tm, "[%Y,%m,%d,%H,%M,%S]");
    return buffer.str();
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

void EpochServer::log(const std::string& log) {
    INFO(log);
}

#define SET_RESULT(x,y) outCode = x; out = y;
#define STR_MOVE(x) std::move(std::string(x))
#define THROW_ARGS_INVALID_NUM(x) throw std::runtime_error(std::string("Invalid number of args for ") + x)

// TODO test if moving args works
int EpochServer::callExtensionEntrypoint(char *output, int outputSize, const char *function, const char **args, int argsCnt) {

    std::string out;
    int outCode = 0;

    size_t fncLen = strlen(function);

    try {
        if (fncLen == 2) {
            this->callExtensionEntrypointByNumber(out, outputSize, outCode, function, args, argsCnt);
        }
        else if (!strncmp(function, "db", 2)) {
            this->dbEntrypoint(out, outputSize, outCode, function, args, argsCnt);
        }
        else if (!strncmp(function, "be", 2)) {
            this->beEntrypoint(out, outputSize, outCode, function, args, argsCnt);
        }
        else if (!strcmp(function, "playerCheck")) {
            this->callExtensionEntrypointByNumber(out, outputSize, outCode, "30", args, argsCnt);
        }
        else if (!strcmp(function, "log")) {
            this->callExtensionEntrypointByNumber(out, outputSize, outCode, "91", args, argsCnt);
        }
        else if (!strcmp(function, "getCurrentTime")) {
            this->callExtensionEntrypointByNumber(out, outputSize, outCode, "00", args, argsCnt);
        }
        else if (!strcmp(function, "getRandomString")) {
            this->callExtensionEntrypointByNumber(out, outputSize, outCode, "01", args, argsCnt);
        }
        else if (!strcmp(function, "getStringMd5")) {
            this->callExtensionEntrypointByNumber(out, outputSize, outCode, "02", args, argsCnt);
        }
        else if (!strcmp(function, "version")) {
            this->callExtensionEntrypointByNumber(out, outputSize, outCode, "90", args, argsCnt);
        }
        else {
            SET_RESULT(1, "Unknown function");
        }
    }
    catch (std::exception& e) {
        SET_RESULT(outCode ? outCode : 1, static_cast<std::string>("Error occured: ") + e.what());
    }

    strncpy_s(output, outputSize, out.c_str(), _TRUNCATE);
    return outCode;
}

void EpochServer::callExtensionEntrypointByNumber(std::string& out, int outputSize, int& outCode, const char *function, const char **args, int argsCnt) {
    switch (function[0]) {
        // extension info
        case '9': {
            switch (function[1]) {
                // extension callback poll
                case '0': {
                    if (this->resultsMutex.try_lock()) {
                        if (this->results.empty()) {
                            outCode = 101; // Empty
                        }
                        else {
                            try {
                                auto& res = this->results.front();
                                if ((res.first.size() - res.second) > outputSize) {
                                    // split up
                                    out = std::string(res.first, res.second, outputSize);
                                    outCode = 100; // Split msg
                                    res.second += outputSize;
                                }
                                else {
                                    // done -> pop
                                    out = std::string(res.first, res.second);
                                    this->results.pop();
                                }
                            }
                            catch (...) {
                                outCode = 103; // Error
                            }
                        }
                        this->resultsMutex.unlock();
                    }
                    else {
                        outCode = 102; // Busy
                    }
                    break;
                }
                // log
                case '1': {
                    if (argsCnt < 1) throw std::runtime_error("Nothing to log was provided");

                    std::string msg = args[0];
                    for (int i = 1; i < argsCnt; ++i) {
                        msg += "\n";
                        msg += args[i];
                    }

                    threadpool->fireAndForget([x = std::move(msg)]() {
                        INFO(x);
                    });
                    break;
                }
                default: { SET_RESULT(1, "Unknown function"); }
            }
            break;
        }
    // db
    case '1': {
        this->dbEntrypoint(out, outputSize, outCode, function[1], args, argsCnt);
        break;
    }
    // be
    case '2': {
        this->beEntrypoint(out, outputSize, outCode, function[1], args, argsCnt);
        break;
    }
    // steamapi
    case '3': {
        switch (function[1]) {
            // playercheck
            case '0': {
                if (argsCnt < 1) throw std::runtime_error("No playerid given to check");
                if (strlen(args[0]) != 17) throw std::runtime_error("Playerid is not a steam64id. Length mismatch");
                if (!this->steamApi) {
                    SET_RESULT(1, "STEAMAPI NOT AVAILABLE");
                    return;
                }
                if (!this->rcon) {
                    SET_RESULT(1, "RCON NOT AVAILABLE");
                    return;
                }
                threadpool->fireAndForget([this, x = STR_MOVE(args[0])]() {
                    std::string err;
                    if (this->steamApi && this->rcon && !this->steamApi->initialPlayerCheck(x, err)) {
                        auto guid = this->__getBattlEyeGUID(std::stoull(x));
                        this->rcon->add_ban(guid);
                        this->rcon->kick(guid);
                    }
                });
                break;
            }
            default: { SET_RESULT(1, "Unknown function"); }
        }
        break;
    }
    
    /////////////////////////////
    // 4-8 TODO
    /////////////////////////////


    // util
    case '0': {
        switch (function[1]) {
            // current time
            case '0': {
                SET_RESULT(0, this->getCurrentTime());
                break;
            }
            // random string
            case '1': {
                SET_RESULT(0, this->getRandomString());
                break;
            }
            // string md5
            case '2': {
                if (argsCnt < 1) throw std::runtime_error("No string to hash was given");
                SET_RESULT(0, this->getStringMd5(args[0]));
                break;
            }
            default: { SET_RESULT(1, "Unknown function"); }
        }
        break;
    }
    default: { SET_RESULT(1, "Unknown function"); }
    };
}

void EpochServer::dbEntrypoint(std::string& out, int outputSize, int& outCode, const char function, const char **args, int argsCnt) {
    switch(function) {
        // get
        case '1': {
            if (argsCnt == 2) {
                this->dbManager->get<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), std::nullopt, std::nullopt);
            }
            else if (argsCnt >= 3) {
                this->dbManager->get<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), STR_MOVE(args[2]), argsCnt >= 4 ? STR_MOVE(args[3]) : "[]");
            }
            else THROW_ARGS_INVALID_NUM("get");
            break;
        };
                  // getTtl
        case '2': {
            if (argsCnt == 2) {
                this->dbManager->getWithTtl<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), std::nullopt, std::nullopt);
            }
            else if (argsCnt >= 3) {
                this->dbManager->getWithTtl<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), STR_MOVE(args[2]), argsCnt >= 4 ? STR_MOVE(args[3]) : "[]");
            }
            else THROW_ARGS_INVALID_NUM("getTtl");
            break;
        };
                  // set
        case '3': {
            if (argsCnt == 3) {
                this->dbManager->set<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), STR_MOVE(args[2]), std::nullopt, std::nullopt);
            }
            else if (argsCnt >= 4) {
                this->dbManager->set<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), STR_MOVE(args[2]), STR_MOVE(args[3]), argsCnt >= 5 ? STR_MOVE(args[4]) : "[]");
            }
            else THROW_ARGS_INVALID_NUM("set");
            break;
        };
                  // setEx
        case '4': {
            if (argsCnt == 4) {
                int ttl = std::stoi(args[2]);
                this->dbManager->setEx<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), ttl, STR_MOVE(args[3]), std::nullopt, std::nullopt);
            }
            else if (argsCnt >= 5) {
                int ttl = std::stoi(args[2]);
                this->dbManager->setEx<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), ttl, STR_MOVE(args[3]), STR_MOVE(args[4]), argsCnt >= 6 ? STR_MOVE(args[5]) : "[]");
            }
            else THROW_ARGS_INVALID_NUM("setEx");
            break;
        };
                  // query
        case '5': {
            break;
        };
                  // Exists
        case '6': {
            if (argsCnt == 2) {
                this->dbManager->exists<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), std::nullopt, std::nullopt);
            }
            else if (argsCnt >= 3) {
                this->dbManager->exists<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), STR_MOVE(args[2]), argsCnt >= 4 ? STR_MOVE(args[3]) : "[]");
            }
            else THROW_ARGS_INVALID_NUM("exists");
            break;
        };
                  // Expire
        case '7': {
            if (argsCnt == 3) {
                int ttl = std::stoi(args[2]);
                this->dbManager->expire<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), ttl, std::nullopt, std::nullopt);
            }
            else if (argsCnt >= 4) {
                int ttl = std::stoi(args[2]);
                this->dbManager->expire<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), ttl, STR_MOVE(args[3]), argsCnt >= 5 ? STR_MOVE(args[4]) : "[]");
            }
            else THROW_ARGS_INVALID_NUM("expire");
            break;
        };
                  // del
        case '8': {
            if (argsCnt == 2) {
                this->dbManager->del<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), std::nullopt, std::nullopt);
            }
            else if (argsCnt >= 3) {
                this->dbManager->del<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), STR_MOVE(args[2]), argsCnt >= 4 ? STR_MOVE(args[3]) : "[]");
            }
            else THROW_ARGS_INVALID_NUM("del");
            break;
        };
                  // getRange
        case '9': {
            if (argsCnt == 4) {
                unsigned long from, to;
                from = std::stoul(args[2]);
                to = std::stoul(args[3]);
                this->dbManager->getRange<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), from, to, std::nullopt, std::nullopt);
            }
            else if (argsCnt >= 5) {
                unsigned long from, to;
                from = std::stoul(args[2]);
                to = std::stoul(args[3]);
                this->dbManager->getRange<DBExecutionType::ASYNC_CALLBACK>(args[0], STR_MOVE(args[1]), from, to, STR_MOVE(args[4]), argsCnt >= 6 ? STR_MOVE(args[5]) : "[]");
            }
            else THROW_ARGS_INVALID_NUM("getRange");
            break;
        };
                  // TODO
        case '0': {
            break;
        };
        default: { SET_RESULT(1, "Unknown function"); };
    };
}

#define MAP_DB_ENTRY(name, number)\
if (!strcmp(function, name)) {\
    this->dbEntrypoint(out, outputSize, outCode, number, args, argsCnt);\
    return;\
}

void EpochServer::dbEntrypoint(std::string& out, int outputSize, int& outCode, const char *function, const char **args, int argsCnt) {
    
    // skip prefix TODO test encoding.. might need to skipp more bytes
    function += 2;

    MAP_DB_ENTRY("Get", '1');
    MAP_DB_ENTRY("Exists", '6');
    MAP_DB_ENTRY("Set", '3');
    MAP_DB_ENTRY("SetEx", '4');
    MAP_DB_ENTRY("Expire", '7');
    MAP_DB_ENTRY("Del", '8');
    MAP_DB_ENTRY("Query", '5');
    MAP_DB_ENTRY("GetTtl", '2');
    MAP_DB_ENTRY("GetRange", '9');
    
    if (!strcmp(function, "Ping")) { // TODO callback
        this->dbManager->ping<DBExecutionType::ASYNC_CALLBACK>(args[0], std::nullopt, std::nullopt);
    }
    else {
        SET_RESULT(1, "Unknown db command");
    }
}

void EpochServer::beEntrypoint(std::string& out, int outputSize, int& outCode, const char function, const char **args, int argsCnt) {
    
    if (!this->rcon) {
        SET_RESULT(1, "RCON NOT AVAILABLE");
        return;
    }

    switch (function) {
        // broadcast
        case '0': {
            if (argsCnt < 1) throw std::runtime_error("Missing message param for beBroadcastMessage");
            threadpool->fireAndForget([this, x = std::string(args[0])]() {
                this->rcon->send_global_msg(x);
            });
            break;
        }
        // kick
        case '1': {
            if (argsCnt < 1) throw std::runtime_error("Missing guid param in beKick");
            threadpool->fireAndForget([this, x = std::string(args[0])]() {
                this->rcon->kick(x);
            });
            break;
        }
        // ban
        case '2': {
            if (argsCnt < 1) throw std::runtime_error("Missing params for beBan");
            std::string msg = argsCnt > 1 ? args[1] : "BE Ban";
            if (msg.empty()) msg = "BE Ban";
            int dur = -1;
            if (argsCnt > 2) {
                try {
                    dur = std::stoi(args[2]);
                }
                catch (...) {
                    WARNING(std::string("Could not parse banDuration, fallback to permaban. GUID: ") + args[0]);
                }
            }
            threadpool->fireAndForget([this, uid = std::string(args[0]), msg = std::move(msg), dur]() {
                this->rcon->add_ban(uid, msg, dur);
            });
            break;
        }
        // lock
        case '3': {
            threadpool->fireAndForget([this]() {
                this->rcon->lockServer();
            });
            break;
        }
        // unlock
        case '4': {
            threadpool->fireAndForget([this]() {
                this->rcon->unlockServer();
            });
            break;
        }
        // shutdown
        case '5': {
            this->rcon->shutdownServer();
            break;
        }
        default: {
            SET_RESULT(1, "Unknown be command");
        }
    }
}

#define MAP_BE_ENTRY(name, number)\
if (!strcmp(function, name)) {\
    this->beEntrypoint(out, outputSize, outCode, number, args, argsCnt);\
    return;\
}

void EpochServer::beEntrypoint(std::string& out, int outputSize, int& outCode, const char *function, const char **args, int argsCnt) {

    if (!this->rcon) {
        SET_RESULT(1, "RCON NOT AVAILABLE");
        return;
    }

    MAP_BE_ENTRY("beBroadcastMessage", '0');
    MAP_BE_ENTRY("beKick", '1');
    MAP_BE_ENTRY("beBan", '2');
    MAP_BE_ENTRY("beShutdown", '5');
    MAP_BE_ENTRY("beLock", '3');
    MAP_BE_ENTRY("beUnlock", '4');

    // No matches
    SET_RESULT(1, "Unknown be command");

}
