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
    
    Whitelist(std::shared_ptr<RCON> rcon) : rcon(rcon) {
        if (!rcon) throw std::runtime_error("Whitelist only works with working RCon!");

        this->rcon->set_player_connected_callback([this](const RconPlayerInfo& info) {
            this->on_player_connected(info);
        });

        this->rcon->set_player_disconnected_callback([this](const RconPlayerInfo& info) {
            this->on_player_disconnected(info);
        });

        this->rcon->set_player_connected_callback([this](const RconPlayerInfo& info) {
            this->on_player_verified(info);
        });
    }

    void set_whitelist_enabled(bool _state) {
        this->enable_whitelist = _state;
    }

    void set_whitelist(const std::vector<std::string>& _guids) {
        std::lock_guard<std::mutex> lock(whitelist_mutex);
        this->whitelisted_guids = std::unordered_set<std::string>(_guids.begin(),_guids.end());
    }

    void set_open_slots(int _slots) {
        this->open_slots = _slots;
    }

    void set_white_list_kick_message(const std::string& _msg) {
        this->kick_message_wl = _msg;
    }

    void add_to_whitelist(const std::string& _guid) {
        std::lock_guard<std::mutex> lock(whitelist_mutex);
        this->whitelisted_guids.insert(_guid);
    }

    void remove_from_whitelist(const std::string& _guid) {
        std::lock_guard<std::mutex> lock(whitelist_mutex);
        this->whitelisted_guids.erase(_guid);
    }

    void set_max_players(int _players) {
        this->max_players = _players;
    }

    // VPN DETECT

    void enable_vpn_detection(const std::string& api_key, bool flag_if_suspecious = false) {
        this->enable_vpnchecks = true;

        this->vpn = std::make_shared<VPNDetection>(api_key, flag_if_suspecious, true);
    }

    void set_vpn_detect_kick_msg(const std::string& _msg) {
        this->kick_message_vpn = _msg;
    }

    void add_vpn_detection_guid_exception(const std::string& _guid) {
        std::lock_guard<std::mutex> lock(vpn_whitelist_mutex);
        this->vpn_whitelisted_guids.insert(_guid);
    }

    // rcon callbacks
    void on_player_connected(const RconPlayerInfo& info) {

        // name checks
        bool bad_name = this->is_bad_player_string(info.player_name);
        if (bad_name) {
            this->rcon->send_command("kick " + info.number + " Bad Playername! Only A-Z,a-z,0-9 allowed!");
            return;
        }

    }

    void on_player_verified(const RconPlayerInfo& info) {

        //check whitelist
        if (info.verified && this->enable_whitelist && !this->is_whitelisted_player(info.guid)) {
            this->open_slots--;
            if (this->open_slots < 0) {
                this->rcon->send_command("kick " + info.number + " " + this->kick_message_wl);
                return;
            }
        }

        // vpn check
        if (this->enable_vpnchecks && info.ip.length() > 1 && !this->is_vpn_whitelisted_player(info.guid)) {

            threadpool->fireAndForget([this, ip = info.ip, number = info.number ]() {
                bool vpn_detected = false;
                bool request_throtteled = false;
                this->vpn->check_vpn(ip, vpn_detected, request_throtteled);

                if (vpn_detected) {
                    this->rcon->send_command("kick " + number + " " + this->kick_message_vpn);
                }
            });

        }
    }

    void on_player_disconnected(const RconPlayerInfo& info) {

        // free open slot
        if (info.verified && !this->is_whitelisted_player(info.guid)) {
            this->open_slots++;
        }

    }
};

#endif // !__WHITELIST_HPP__
