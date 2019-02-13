#ifndef __STEAMAPI_H__
#define __STEAMAPI_H__

#include <string>
#include <vector>
#include <shared_mutex>

#include <main.hpp>

// https://wiki.teamfortress.com/wiki/WebAPI/GetPlayerSummaries
struct SteamPlayerSummary {
    unsigned long steamid;
    short communityvisibilitystate;
    short profilestate;
    std::string personaname;
    long lastlogoff;
    std::string profileurl;
    short personastate;

    //optional
    long long timecreated;
};

// https://wiki.teamfortress.com/wiki/WebAPI/GetPlayerBans
struct SteamPlayerBans {
    unsigned long steamId;
    bool CommunityBanned;
    bool VACBanned;
    short NumberOfGameBans;
    short DaysSinceLastBan;
    std::string EconomyBan;
};

typedef std::pair< std::string, std::string> RequestParam;

struct SteamAPIResponse {
    int status;
    std::string content;
};

class SteamAPI {
private:
    std::string _apiKey;

    SteamAPIResponse _sendRequest(const std::string& path, const std::vector<RequestParam>& params);

    std::unordered_map<std::string, SteamPlayerSummary> summaryCache;
    std::mutex summaryMutex;

    std::unordered_map<std::string, SteamPlayerBans> bansCache;
    std::mutex bansMutex;

    /**
    * steam id check cache
    **/
    std::shared_mutex checkedSteamIdsMutex;
    std::vector< std::pair<uint64, bool> > checkedSteamIds;

    /**
    * steam api config
    **/
    short steamApiLogLevel = 0;
    bool kickVacBanned = false;
    short minDaysSinceLastVacBan = 0;
    short maxVacBans = 1;
    int minAccountAge = 0; // in days

public:

    SteamAPI(const SteamAPI&) = delete;
    SteamAPI& operator=(const SteamAPI&) = delete;
    SteamAPI(SteamAPI&&) = delete;
    SteamAPI& operator=(SteamAPI&&) = delete;

    SteamAPI(const std::string& APIKey);
    ~SteamAPI();

    /**
    *  \brief Initial player check when joining
    *  
    **/
    SteamPlayerBans GetPlayerBans(const std::string& steamId, bool force = false);

    /**
    *  \brief Initial player check when joining
    *  
    **/
    SteamPlayerSummary GetPlayerSummaries(const std::string& steamId, bool force = false);

    /**
    *  \brief Initial player check when joining
    *  \param steamId 64Bit Steam community id
    *  \return bool true if player is ok to join, false if some condition was not fulfilled, msg is written to errorMsg
    **/
    bool initialPlayerCheck(const std::string& steamId, std::string& errorMsg);

    /**
    *   Setters
    *
    */
    void setSteamApiLogLevel(short steamApiLogLevel) {
        this->steamApiLogLevel = steamApiLogLevel;
    }

    void setKickVacBanned(bool kickVacBanned) {
        this->kickVacBanned = kickVacBanned;
    }

    void setMaxVacBans(short maxVacBans) {
        this->maxVacBans = maxVacBans;
    }

    void setMinAccountAge(short minAccountAge) {
        this->minAccountAge = minAccountAge;
    }

    void setMinDaysSinceLastVacBan(int minDaysSinceLastVacBan) {
        this->minDaysSinceLastVacBan = minDaysSinceLastVacBan;
    }
};

#endif