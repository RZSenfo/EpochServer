#include <httplib.h>
#undef GetObject
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/error/error.h>

#include <sstream>
#include <fstream>


#include <SteamAPI/SteamAPI.hpp>

SteamAPI::SteamAPI(const std::string& _apiKey) {
    this->_apiKey = _apiKey;
}

SteamAPI::~SteamAPI() {
}

SteamPlayerBans SteamAPI::GetPlayerBans(const std::string& _steamId, bool force) {
    
    if (!force) {
        std::unique_lock<std::mutex> lock(this->bansMutex);

        try {
            return this->bansCache.at(_steamId);
        } catch (...) {}
    }


    auto resp = this->_sendRequest("/ISteamUser/GetPlayerBans/v1/", { RequestParam("steamids", _steamId) });

    SteamPlayerBans response;

    if (resp.status == 200) {

        rapidjson::Document document;
        rapidjson::ParseResult ok = document.Parse(resp.content.c_str());

        if (ok) {

            auto players = document["players"].GetArray();
            if (players.Size() > 0) {
                auto player = players[0].GetObject();

                response.DaysSinceLastBan = player["DaysSinceLastBan"].GetInt();
                response.CommunityBanned = player["CommunityBanned"].GetBool();
                response.EconomyBan = player["EconomyBan"].GetString();
                response.NumberOfGameBans = player["NumberOfGameBans"].GetInt();
                response.steamId = player["steamId"].GetUint64();
                response.VACBanned = player["VACBanned"].GetBool();
            }
        }
    }

    {
        std::unique_lock<std::mutex> lock(this->bansMutex);
        try {
            this->bansCache.insert({ _steamId, response });
        }
        catch (...) {}
    }

    return response;

}

SteamPlayerSummary SteamAPI::GetPlayerSummaries(const std::string& _steamId, bool force) {
    
    if (!force) {
        std::unique_lock<std::mutex> lock(this->summaryMutex);

        try {
            return this->summaryCache.at(_steamId);
        }
        catch (...) {}
    }

    auto resp = this->_sendRequest("/ISteamUser/GetPlayerSummaries/v0002/", {});

    SteamPlayerSummary response;

    if (resp.status == 200) {

        rapidjson::Document document;
        rapidjson::ParseResult ok = document.Parse(resp.content.c_str());

        if (ok) {

            auto players = document["players"].GetArray();
            if (players.Size() > 0) {
                auto player = players[0].GetObject();
                
                response.communityvisibilitystate = player["communityvisibilitystate"].GetInt();
                response.lastlogoff = player["lastlogoff"].GetUint64();
                response.personaname = player["personaname"].GetString();
                response.personastate = player["personastate"].GetInt();
                response.profilestate = player["profilestate"].GetInt();
                response.profileurl = player["profileurl"].GetString();
                response.steamid = player["steamid"].GetUint64();

                if (player.HasMember("timecreated")) {
                    response.timecreated = player["timecreated"].GetUint64();
                }
                else {
                    response.timecreated = -1;
                }
            }
        }
    }

    {
        std::unique_lock<std::mutex> lock(this->summaryMutex);
        try {
            this->summaryCache.insert({ _steamId, response });
        }
        catch (...) {}
    }

    return response;

}

SteamAPIResponse SteamAPI::_sendRequest(const std::string& path, const std::vector<RequestParam>& params) {

    // Add API key
    std::string fullpath = path + "?key=" + this->_apiKey;

    // Build query
    for (auto& x : params) {
        fullpath += "&" + x.first + "=" + x.second;
    }
    
    // Setup HTTP request
    httplib::Client clt("api.steampowered.com");

    auto response = clt.Get(fullpath.c_str());

    SteamAPIResponse resp;
    resp.content = response->body;
    resp.status = response->status;

    return resp;
}

bool SteamAPI::initialPlayerCheck(const std::string& steamIdStr, std::string& errorMsg) {

    auto _steamId = std::stoull(steamIdStr);
    {
        std::shared_lock<std::shared_mutex> lock(this->checkedSteamIdsMutex);
        for (auto& x : this->checkedSteamIds) {
            if (x.first == _steamId) {
                if (x.second) errorMsg = "KICKED";
                return x.second;
            }
        }
    }
    bool kick = false;

    auto bans = this->GetPlayerBans(steamIdStr);

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

            errorMsg = "VAC BANNED";
            kick = true;
        }
        if (!kick && this->minDaysSinceLastVacBan > 0 && bans.DaysSinceLastBan < this->minDaysSinceLastVacBan) {
            if (this->steamApiLogLevel >= 1) {
                std::stringstream log;
                log << "[SteamAPI] VAC ban " << _steamId << std::endl;
                log << "- DaysSinceLastBan: " << bans.DaysSinceLastBan;
                INFO(log.str());
            }

            errorMsg = "VAC BANNED RECENTLY";
            kick = true;
        }
        if (!kick && this->maxVacBans > 0 && bans.NumberOfGameBans >= this->maxVacBans) {
            if (this->steamApiLogLevel >= 1) {
                std::stringstream log;
                log << "[SteamAPI] VAC ban " << _steamId << std::endl;
                log << "- NumberOfVACBans: " << bans.NumberOfGameBans;
                INFO(log.str());
            }

            errorMsg = "TOO MANY VAC BANS";
            kick = true;
        }
    }

    // Player check
    auto summary = this->GetPlayerSummaries(steamIdStr);
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

                errorMsg = "STEAM ACCOUNT NOT OLD ENOUGH";
                kick = true;
            }
        }
    }

    {
        std::unique_lock<std::shared_mutex> lock(this->checkedSteamIdsMutex);
        this->checkedSteamIds.push_back({ _steamId, !kick });
    }

    return !kick;
}
