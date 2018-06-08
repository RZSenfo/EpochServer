#ifndef __STEAMAPI_H__
#define __STEAMAPI_H__

#include <string>
#include <happyhttp.h>
#include "rapidjson/document.h"


struct SteamAPIResponseContent {
    short int Status;
    size_t ByteCount;
    std::string Content;
};
typedef std::map<std::string, std::string> SteamAPIQuery;

class SteamAPI {
public:
    std::string _apiKey;
    SteamAPIResponseContent *_responseContent;

    bool _sendRequest(std::string URI, SteamAPIQuery Query);

public:
    SteamAPI(std::string APIKey);
    ~SteamAPI();

    bool GetPlayerBans(std::string SteamIds, rapidjson::Document *Document);
    bool GetPlayerSummaries(std::string SteamIds, rapidjson::Document *Document);
};

#endif