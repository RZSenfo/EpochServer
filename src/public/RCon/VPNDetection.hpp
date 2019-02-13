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

    VPNDetection(const VPNDetection&) = delete;
    VPNDetection& operator=(const VPNDetection&) = delete;
    VPNDetection(VPNDetection&&) = delete;
    VPNDetection& operator=(VPNDetection&&) = delete;

    /**
    *   \brief tries to detect usage of vpn
    **/
    void check_vpn(const std::string& ip, bool& vpn_detected, bool& request_throtteled);

    std::optional<IPInfo> check_ip_cache(const std::string& ip);

    VPNDetection(const std::string& api_key, bool flag_if_suspecious = false, bool enable = true);
};

#endif // !__VPN_DETECTION_HPP__
