#ifndef __STEAMAPI_H__
#define __STEAMAPI_H__

#include <string>
#include <vector>

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

public:
    SteamAPI(const std::string& APIKey);
    ~SteamAPI();

    SteamPlayerBans GetPlayerBans(const std::string& steamIds);
    SteamPlayerSummary GetPlayerSummaries(const std::string& steamIds);
};

#endif