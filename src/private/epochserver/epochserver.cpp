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

// TODO test if moving args works
int EpochServer::callExtensionEntrypoint(char *output, int outputSize, const char *function, const char **args, int argsCnt) {

    std::string out;
    int outCode = 0;

    size_t fncLen = strlen(function);

    try {
        if (fncLen == 3) {
            this->callExtensionEntrypointByNumber(out, outCode, function, args, argsCnt);
        }
        else if (!strncmp(function, "db", 2)) {
            this->dbEntrypoint(out, outCode, function, args, argsCnt);
        }
        else if (!strncmp(function, "be", 2)) {
            this->beEntrypoint(out, outCode, function, args, argsCnt);
        }
        else if (!strcmp(function, "playerCheck")) {
            this->callExtensionEntrypointByNumber(out, outCode, "300", args, argsCnt);
        }
        else if (!strcmp(function, "extLog")) {
            this->callExtensionEntrypointByNumber(out, outCode, "910", args, argsCnt);
        }
        else if (!strcmp(function, "getCurrentTime")) {
            this->callExtensionEntrypointByNumber(out, outCode, "000", args, argsCnt);
        }
        else if (!strcmp(function, "getRandomString")) {
            this->callExtensionEntrypointByNumber(out, outCode, "010", args, argsCnt);
        }
        else if (!strcmp(function, "getStringMd5")) {
            this->callExtensionEntrypointByNumber(out, outCode, "020", args, argsCnt);
        }
        else if (!strcmp(function, "version")) {
            this->callExtensionEntrypointByNumber(out, outCode, "900", args, argsCnt);
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

void EpochServer::callExtensionEntrypointByNumber(std::string& out, int& outCode, const char *function, const char **args, int argsCnt) {
    switch (function[0]) {
    // db
    case '1': {
        this->dbEntrypoint(out, outCode, function, args, argsCnt);
        break;
    }
    // be
    case '2': {
        this->beEntrypoint(out, outCode, function, args, argsCnt);
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
                threadpool->fireAndForget([this, x = std::move(std::string(args[0]))]() {
                    std::string err;
                    if (this->steamApi && this->rcon && !this->steamApi->initialPlayerCheck(x, err)) {
                        this->rcon->add_ban(this->__getBattlEyeGUID(std::stoull(x)));
                        this->rcon->kick(this->__getBattlEyeGUID(std::stoull(x)));
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
    // extension info
    case '9': {
        switch (function[1]) {
            // version
            case '0': {
                SET_RESULT(0, EXTENSION_VERSION);
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
    default: { SET_RESULT(1, "Unknown function"); }
    };
}

void EpochServer::dbEntrypoint(std::string& out, int& outCode, const char *function, const char **args, int argsCnt) {
    
    if (strlen(function) == 3) {
        switch(function[1]) {
            // Poll
            case '0': {
                if (argsCnt < 2 || strlen(args[0]) == 0 || strlen(args[1]) == 0) {
                    throw std::runtime_error("Invalid args for dbPoll");
                }

                unsigned long id = std::stoul(args[1]);

                try {
                    auto res = dbManager->tryPopResult(args[0], id);
                    if (res.index() == 0) {
                        SET_RESULT(0, std::get<std::string>(res));
                    }
                    else if (res.index() == 1) {
                        SET_RESULT(0, std::to_string(std::get<bool>(res)));
                    }
                    else if (res.index() == 2) {
                        SET_RESULT(0, std::to_string(std::get<int>(res)));
                    }
                    else if (res.index() == 3) {
                        auto result = std::get< std::pair<std::string, int> >(res);
                        SET_RESULT(0, "[" + result.first + "," + std::to_string(result.second) + "]");
                    }
                    else if (res.index() == 4) {
                        auto result = std::get< std::vector<std::string> >(res);
                        std::string str;
                        if (result.empty()) {
                            str = "[]";
                        }
                        else {
                            str = "[";
                            for (auto& x : result) {
                                str += x;
                                str += ',';
                            }
                            str[str.length() - 1] = ']';
                        }
                        SET_RESULT(0, str);
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
                break;
            };
            // get
            case '1': {
                if (argsCnt < 2) throw std::runtime_error("Not enough args given for get");
                unsigned long id = this->dbManager->get<DBExecutionType::ASYNC_POLL>(args[0], args[1]);
                SET_RESULT(0, std::to_string(id));
                break;
            };
            // getTtl
            case '2': {
                if (argsCnt < 2) throw std::runtime_error("Not enough args given for getTtl");
                unsigned long id = this->dbManager->getWithTtl<DBExecutionType::ASYNC_POLL>(args[0], args[1]);
                SET_RESULT(0, std::to_string(id));
                break;
            };
            // set
            case '3': {
                if (argsCnt < 3) throw std::runtime_error("Not enough args given for set");
                this->dbManager->set<DBExecutionType::ASYNC_CALLBACK>(args[0], args[1], args[2], std::nullopt, std::nullopt);
                break;
            };
            // setEx
            case '4': {
                if (argsCnt < 4) throw std::runtime_error("Not enough args given for setEx");
                int ttl = std::stoi(args[2]);
                this->dbManager->setEx<DBExecutionType::ASYNC_CALLBACK>(args[0], args[1], ttl, args[3], std::nullopt, std::nullopt);
                break;
            };
            // query
            case '5': {
                break;
            };
            // Exists
            case '6': {
                if (argsCnt < 2) throw std::runtime_error("Not enough args given for exists");
                unsigned long id = this->dbManager->exists<DBExecutionType::ASYNC_POLL>(args[0], args[1]);
                SET_RESULT(0, std::to_string(id));
                break;
            };
            // Expire
            case '7': {
                if (argsCnt < 2) throw std::runtime_error("Not enough args given for expire");
                int ttl = std::stoi(args[2]);
                this->dbManager->expire<DBExecutionType::ASYNC_CALLBACK>(args[0], args[1], ttl, std::nullopt, std::nullopt);
                break;
            };
            // del
            case '8': {
                if (argsCnt < 2) throw std::runtime_error("Not enough args given for del");
                this->dbManager->del<DBExecutionType::ASYNC_CALLBACK>(args[0], args[1], std::nullopt, std::nullopt);
                break;
            };
            // getRange
            case '9': {
                if (argsCnt < 4) throw std::runtime_error("Not enough args given for getRange");
                unsigned long from, to;
                from = std::stoul(args[2]);
                to = std::stoul(args[3]);
                unsigned long id = this->dbManager->getRange<DBExecutionType::ASYNC_POLL>(args[0], args[1], from, to);
                SET_RESULT(0, std::to_string(id));
                break;
            };
            default: { SET_RESULT(1, "Unknown function"); };
        };
    }
    else {
        // skip prefix TODO test encoding.. might need to skipp more bytes
        function += 2;

        if (!strcmp(function, "Poll")) {
            // 0
            this->dbEntrypoint(out, outCode, "100", args, argsCnt);
        }
        else if (!strcmp(function, "Get")) {
            // 1
            this->dbEntrypoint(out, outCode, "110", args, argsCnt);
        }
        else if (!strcmp(function, "Exists")) {
            // 6
            this->dbEntrypoint(out, outCode, "160", args, argsCnt);
        }
        else if (!strcmp(function, "Set")) {
            // 3
            this->dbEntrypoint(out, outCode, "130", args, argsCnt);
        }
        else if (!strcmp(function, "SetEx")) {
            // 4
            this->dbEntrypoint(out, outCode, "140", args, argsCnt);
        }
        else if (!strcmp(function, "Expire")) {
            // 7
            this->dbEntrypoint(out, outCode, "170", args, argsCnt);
        }
        else if (!strcmp(function, "Del")) {
            // 8
            this->dbEntrypoint(out, outCode, "180", args, argsCnt);
        }
        else if (!strcmp(function, "Query")) {
            // TODO
            SET_RESULT(1, "TODO");
        }
        else if (!strcmp(function, "GetTtl")) {
            // 2
            this->dbEntrypoint(out, outCode, "120", args, argsCnt);
        }
        else if (!strcmp(function, "SetSync")) {
            if (argsCnt < 3) throw std::runtime_error("Not enough args given for set");
            unsigned long id = this->dbManager->set<DBExecutionType::ASYNC_POLL>(args[0], args[1], args[2]);
            SET_RESULT(0, std::to_string(id));
        }
        else if (!strcmp(function, "SetExSync")) {
            if (argsCnt < 4) throw std::runtime_error("Not enough args given for setEx");
            int ttl = std::stoi(args[2]);
            unsigned long id = this->dbManager->setEx<DBExecutionType::ASYNC_POLL>(args[0], args[1], ttl, args[3]);
            SET_RESULT(0, std::to_string(id));
        }
        else if (!strcmp(function, "ExpireSync")) {
            if (argsCnt < 2) throw std::runtime_error("Not enough args given for expire");
            int ttl = std::stoi(args[2]);
            unsigned long id = this->dbManager->expire<DBExecutionType::ASYNC_POLL>(args[0], args[1], ttl);
            SET_RESULT(0, std::to_string(id));
        }
        else if (!strcmp(function, "DelSync")) {
            if (argsCnt < 2) throw std::runtime_error("Not enough args given for del");
            unsigned long id = this->dbManager->del<DBExecutionType::ASYNC_POLL>(args[0], args[1]);
            SET_RESULT(0, std::to_string(id));
        }
        else if (!strcmp(function, "GetRange")) {
            // 9
            this->dbEntrypoint(out, outCode, "190", args, argsCnt);
        }
        else if (!strcmp(function, "Ping")) {
            unsigned long id = this->dbManager->ping<DBExecutionType::ASYNC_POLL>(args[0]);
            SET_RESULT(0, std::to_string(id));
        }
        else {
            SET_RESULT(1, "Unknown db command");
        }
    }
}

void EpochServer::beEntrypoint(std::string& out, int& outCode, const char *function, const char **args, int argsCnt) {
    
    if (!this->rcon) {
        SET_RESULT(1, "RCON NOT AVAILABLE");
        return;
    }

    if (strlen(function) == 3) {
        switch (function[1]) {
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
                        WARNING(static_cast<std::string>("Could not parse banDuration, fallback to permaban. GUID: ") + args[0]);
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
    else {
        if (!strcmp(function, "beBroadcastMessage")) {
            this->beEntrypoint(out, outCode, "200", args, argsCnt);
        }
        else if (!strcmp(function, "beKick")) {
            this->beEntrypoint(out, outCode, "210", args, argsCnt);
        }
        else if (!strcmp(function, "beBan")) {
            this->beEntrypoint(out, outCode, "220", args, argsCnt);
        }
        else if (!strcmp(function, "beShutdown")) {
            this->beEntrypoint(out, outCode, "250", args, argsCnt);
        }
        else if (!strcmp(function, "beLock")) {
            this->beEntrypoint(out, outCode, "230", args, argsCnt);
        }
        else if (!strcmp(function, "beUnlock")) {
            this->beEntrypoint(out, outCode, "240", args, argsCnt);
        }
        else {
            SET_RESULT(1, "Unknown be command");
        }
    }
}
