#include <SteamAPI/SteamAPI.hpp>

#include <httplib.h>
#undef GetObject
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/error/error.h>

#include <sstream>
#include <fstream>


SteamAPI::SteamAPI(const std::string& _apiKey) {
    this->_apiKey = _apiKey;
}

SteamAPI::~SteamAPI() {
}

SteamPlayerBans SteamAPI::GetPlayerBans(const std::string& _steamIds) {
    
    auto resp = this->_sendRequest("/ISteamUser/GetPlayerBans/v1/", { RequestParam("steamids", _steamIds) });

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
    return response;

}

SteamPlayerSummary SteamAPI::GetPlayerSummaries(const std::string& _steamIds) {
    
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
