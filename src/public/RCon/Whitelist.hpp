#pragma once

#ifndef __WHITELIST_HPP__
#define __WHITELIST_HPP__

#include <vector>
#include <shared_mutex>
#include <atomic>
#include <unordered_set>

#include <main.hpp>
#include <RCon/RCON.hpp>
#include <RCon/VPNDetection.hpp>

class Whitelist {
private:

    // rcon
    std::shared_ptr<RCON> rcon;

    // vpn
    std::shared_ptr<VPNDetection> vpn;

    // settings - whitelist
    bool enable_whitelist = false;
    int max_players = 100;
    std::atomic<int> open_slots = 0;
    
    std::string kick_message_wl = "Not whitelisted!";
    std::unordered_set<std::string> whitelisted_guids;
    std::mutex whitelist_mutex;

    // settings - vpn checks
    bool enable_vpnchecks = false;
    std::string kick_message_vpn = "VPN detected!";
    std::unordered_set<std::string> vpn_whitelisted_guids;
    std::mutex vpn_whitelist_mutex;

    bool is_bad_player_string(const std::string& player_name) {

        for (int i = 0; i < player_name.length(); i++) {
            char letter = player_name.at(i);
            bool isNumber = letter >= '0' && letter <= '9';
            bool isLetter = isNumber || (letter >= 'A' && letter <= 'Z') || (letter >= 'a' && letter <= 'z');
            bool isBracket = isLetter || letter == '[' || letter == ']' || letter == ' ';
            if (!isBracket) {
                return true;
            }
        }
        return false;

    }

    bool is_whitelisted_player(const std::string& player_guid) {

        if (!this->enable_whitelist) return true;

        std::lock_guard<std::mutex> lock(this->whitelist_mutex);
        return this->whitelisted_guids.count(player_guid) > 0;

    }

    bool is_vpn_whitelisted_player(const std::string& player_guid) {

        std::lock_guard<std::mutex> lock(this->vpn_whitelist_mutex);
        return this->vpn_whitelisted_guids.count(player_guid) > 0;

    }

public:

    Whitelist(const Whitelist&) = delete;
    Whitelist& operator=(const Whitelist&) = delete;
    Whitelist(Whitelist&&) = delete;
    Whitelist& operator=(Whitelist&&) = delete;
    
    Whitelist(std::shared_ptr<RCON> rcon);

    void set_whitelist_enabled(bool _state);

    void set_whitelist(const std::vector<std::string>& _guids);

    void set_open_slots(int _slots);

    void set_white_list_kick_message(const std::string& _msg);

    void add_to_whitelist(const std::string& _guid);

    void remove_from_whitelist(const std::string& _guid);

    void set_max_players(int _players);

    // VPN DETECT

    void enable_vpn_detection(const std::string& api_key, bool flag_if_suspecious = false);

    void set_vpn_detect_kick_msg(const std::string& _msg);

    void add_vpn_detection_guid_exception(const std::string& _guid);

    // rcon callbacks
    void on_player_connected(const RconPlayerInfo& info);

    void on_player_verified(const RconPlayerInfo& info);

    void on_player_disconnected(const RconPlayerInfo& info);
};

#endif // !__WHITELIST_HPP__
