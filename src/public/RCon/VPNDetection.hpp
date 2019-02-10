#pragma once

#ifndef __VPN_DETECTION_HPP__
#define __VPN_DETECTION_HPP__

#include <vector>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <queue>
#include <mutex>

#include <httplib.h>

#undef GetObject
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/error/error.h>
#include <rapidjson/istreamwrapper.h>

struct IPInfo {
    std::string isp;
    std::string country;
    std::string country_code;
    int block = 0;

    IPInfo(const std::string& isp, const std::string& country, const std::string& country_code, int block) : 
        isp(isp), 
        country(country), 
        country_code(country_code), 
        block(block) {}
};

class VPNDetection {
private:

    std::string api_key = "";
    
    bool enable = false;
    bool flag_if_suspecious = false;
    std::unordered_map<std::string, IPInfo> ip_cache;
    std::mutex mutex;

public:

    /**
    *   \brief tries to detect usage of vpn
    **/
    void check_vpn(const std::string& ip, bool& vpn_detected, bool& request_throtteled) {
        
        if (!enable) return;
            
        auto _cache_entry = this->check_ip_cache(ip);
        if (_cache_entry) {

            if (_cache_entry->block == 1) {
                // proxy
                vpn_detected = true;
            }
            else if (_cache_entry->block != 0 && this->flag_if_suspecious) {
                // suspescious
                vpn_detected = true;
            }

        }
        else {

            // Setup HTTP request
            httplib::Client clt("http://v2.api.iphub.info");

            auto response = clt.Get(("/ip/" + ip).c_str(), { { "X-Key", this->api_key } });

            if (response->status == 200 && !response->body.empty()) {
                /*
                {
                "ip": "8.8.8.8",
                "countryCode": "US",
                "countryName": "United States",
                "asn": 15169,
                "isp": "GOOGLE - Google Inc.",
                "block": 1
                }
                */

                rapidjson::Document document;
                document.Parse(response->body.c_str());

                auto ip_info = IPInfo(
                    document["isp"].GetString(),
                    document["countryName"].GetString(),
                    document["countryCode"].GetString(),
                    document["block"].GetInt()
                );

                {
                    std::lock_guard<std::mutex> lock(this->mutex);
                    this->ip_cache.insert_or_assign(ip, ip_info);
                }

                if (ip_info.block == 1) {
                    //proxy
                    vpn_detected = true;
                }
                else if (ip_info.block != 0 && this->flag_if_suspecious) {
                    // suspescious
                    vpn_detected = true;
                }

            }
            else if (response->status == 429) {
                request_throtteled = true;
            }
        }
    }
    
    std::optional<IPInfo> check_ip_cache(const std::string& ip) {
        std::lock_guard<std::mutex> lock(this->mutex);
        try {
            return this->ip_cache.at(ip);
        }
        catch (std::out_of_range& e) {
            return std::nullopt;
        }
    }

    VPNDetection(const std::string& api_key, bool flag_if_suspecious = false, bool enable = true) {
        this->api_key = api_key;
        this->enable = enable;
        this->flag_if_suspecious = flag_if_suspecious;
    }
};

#endif // !__VPN_DETECTION_HPP__
