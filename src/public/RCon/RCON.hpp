#pragma once

#ifndef RCON_HPP
#define RCON_HPP

#include <unordered_map>
#include <mutex>
#include <unordered_set>
#include <optional>
#include <memory>
#include <future>
#include <queue>
#include <cstdio>
#include <cstdlib>
#include <cstdint>  // Includes ::std::uint32_t

#include <main.hpp>
#include <external/UDP.hpp>

#define CRCPP_USE_CPP11
#include <external/crc.hpp>

struct RconPlayerInfo {
    std::string number;
    std::string player_name;
    std::string guid;       //can be wrong until verified
    std::string ip;
    int port;
    bool verified;

    //These are not guaranteed to be correct
    int ping;
    bool lobby;

    //only if vpn check is enabled
    std::string country_code;
    std::string country;
    std::string isp;
};

enum RconTaskType {
    GLOBALMESSAGE,
    KICKALL,
    SHUTDOWN,
    LOCK,
    UNLOCK
};

class RCON {

public:
    
    RCON(const RCON&) = delete;
    RCON& operator=(const RCON&) = delete;
    RCON(RCON&&) = delete;
    RCON& operator=(RCON&&) = delete;

    RCON(const std::string& host, int port, const std::string& pw);
    ~RCON();
    
    // system functions
    void start();
    void enable_auto_reconnect();
    bool is_logged_in();
    std::vector<RconPlayerInfo> get_players();
    size_t get_player_count();
    void set_player_connected_callback(const std::function<void(const RconPlayerInfo&)>& callback);
    void set_player_disconnected_callback(const std::function<void(const RconPlayerInfo&)>& callback);
    void set_player_verified_callback(const std::function<void(const RconPlayerInfo&)>& callback);

    // submit rcon admin command
    void send_command(const std::string& command);

    // common commands
    void send_global_msg(const std::string& msg);
    void remove_ban(const std::string& player_guid);
    void add_ban(const std::string& player_guid);
    void add_ban(const std::string& player_guid, const std::string& reason, int duration = -1);
    void kick(const std::string& player_guid);
    void kick_all();
    void lockServer();
    void unlockServer();
    void shutdownServer();


    // rcon command schedule
    void add_task(const RconTaskType& _type, const std::string& _data, bool _repeat, int _delay, int _initdelay = 0);

private:

    void connect();

    enum message_type {
        LOGIN = 0x00,
        SERVER = 0x01,
        CHAT = 0x02
    };
    struct Packet {
        std::string command;
        message_type type;
    };
    
    // socket
    std::shared_ptr<UDP_Socket> socket;
    
    // endpoint details
    unsigned int port;
    std::string host;
    std::string password;

    void bind();
    bool loggedin = false;

    // heartbeats
    bool send_heart_beat = true;
    std::chrono::time_point<std::chrono::system_clock> last_heart_beat;
    std::chrono::time_point<std::chrono::system_clock> last_ACK;

    // worker thread
    bool run_thread = true;
    std::thread worker;

    // reconnect
    bool auto_reconnect = false;
    short auto_reconnect_trys = 5;
    std::chrono::seconds auto_reconnect_delay = std::chrono::seconds(30);

    // sending packages
    void remove_null_terminator(std::string& _str);
    std::string make_be_message(const std::string&, const message_type&);
    void send_packet(const Packet& rcon_packet);
    void send_packet(const Packet& rcon_packet, std::function<void(int)> _handle_sent);
    
    // internal events
    void handle_disconnect();
    void refresh_players();
    
    void handle_rec(const std::string& received, size_t bytes_received);
    void handle_sent(int bytes_sent);

    // rcon events
    void chat_message(const std::string& _response);
    void on_player_connect(const std::string & _player_number, const std::string & _player_name, const std::string& _ip, int _port);
    void on_player_disconnect(const std::string & _player_number, const std::string & _player_name);
    void on_player_verified_guid(const std::string & _player_number, const std::string & _player_name, const std::string & _player_guid);
    void login_response(const std::string& _response);
    void server_response(const std::string& _response);
    
    // msg cache for split messages
    std::unordered_map< unsigned char, std::pair<unsigned char, std::vector<std::string> > > msg_cache;
    std::mutex msg_cache_mutex;
    char current_seq_num = 0x00;

    void process_message(unsigned char sequence_number, const std::string& message);
    void process_message_players(const std::vector<std::string>& tokens);
    void process_message_missions(const std::vector<std::string>& tokens);
    void process_message_bans(const std::vector<std::string>& tokens);
    
    // Player Name / BEGuid
    std::unordered_map<std::string, RconPlayerInfo> players;
    std::mutex player_mutex;

    /*
    *   FEATURES
    */

    struct Task {
        RconTaskType type;
        std::string data;
        bool repeat;
        int seconds;
        std::chrono::time_point<std::chrono::system_clock> exec_at;
        std::shared_ptr<Task> next_task = nullptr;
    };
    std::shared_ptr<Task> task_head = nullptr;
    std::mutex task_mutex;

    void insert_task(std::shared_ptr<Task> _new_task);

    // Callbacks for events
    std::function<void(const RconPlayerInfo& player_info)> player_connected_callback;
    std::function<void(const RconPlayerInfo& player_info)> player_verified_callback;
    std::function<void(const RconPlayerInfo& player_info)> player_disconnected_callback;

};

#endif // RCON_HPP
