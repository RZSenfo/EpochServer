#include <RCon/RCON.hpp>

void RCON::remove_null_terminator(std::string& _str) {
    if (_str.size() > 0 && _str[_str.size() - 1] == '\0') {
        _str.erase(_str.size() - 1);
    }
}

RCON::RCON(const std::string& _ip, int _port, const std::string& _pw) : 
    host(_ip), 
    port(_port), 
    password(_pw) {

    if (this->host == "") {
        this->host = "127.0.0.1";
    }
    
    if (this->port <= 0) {
        this->port = 2302;
    }
    
    this->remove_null_terminator(this->password);
    
}

void RCON::start() {
    
    this->socket = std::make_shared<UDP_Socket>(host, port, [this]() {
        this->handle_disconnect();
    });

    auto _now = std::chrono::system_clock::now();
    this->last_heart_beat = this->last_ACK = _now - std::chrono::seconds(30);

    if (this->socket->is_connected()) {
        this->bind();
    }
    else {
        this->loggedin = false;
    }

    this->worker = std::thread([this]() {
        std::string cmd = this->make_be_message("", RCON::message_type::SERVER);

        while (this->run_thread) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            //check for ACK of last heart beat
            auto _now = std::chrono::system_clock::now();
            auto _duration = std::chrono::duration_cast<std::chrono::seconds>(_now - last_heart_beat);

            //received no ACK since 5 sec
            if (this->last_heart_beat > this->last_ACK && _duration > std::chrono::seconds(5)) {
                this->handle_disconnect();
            }

            //auto reconnect
            if (!this->loggedin && this->auto_reconnect) {

                while (this->auto_reconnect_trys > 0) {

                    this->socket->reconnect();

                    if (this->socket->is_connected()) {
                        this->bind();
                    }

                    std::this_thread::sleep_for(this->auto_reconnect_delay);

                    if (!this->loggedin) {
                        this->auto_reconnect_trys--;
                    }
                    else {
                        this->auto_reconnect_trys = 5;
                        break;
                    }
                }

            }

            if (this->loggedin) {

                //check heart beat
                _duration = std::chrono::duration_cast<std::chrono::seconds>(_now - this->last_heart_beat);
                if (_duration >= std::chrono::seconds(25)) {
                    this->socket->send(
                        cmd,
                        //handle sent
                        [this](int _bytes) {
                        handle_sent(_bytes);
                    }
                    );
                    this->last_heart_beat = std::chrono::system_clock::now();

                }

                // TODO refresh players in intervall

                //check tasks
                {
                    std::lock_guard<std::mutex> lock(task_mutex);
                    if (this->task_head != nullptr) {
                        if (this->task_head->exec_at <= _now) {
                            switch (this->task_head->type) {
                            case (RconTaskType::GLOBALMESSAGE): {
                                this->send_command("say -1 " + this->task_head->data);
                                break;
                            }
                            case(RconTaskType::KICKALL): {
                                this->kick_all();
                                break;
                            }
                            case(RconTaskType::SHUTDOWN): {
                                std::exit(0);
                                break;
                            }
                            }

                            auto _cur = this->task_head;
                            this->task_head = this->task_head->next_task;

                            if (_cur->repeat) {
                                _cur->exec_at = std::chrono::system_clock::now() + std::chrono::seconds(_cur->seconds);
                                this->insert_task(_cur);
                            }
                        }
                    }
                }

            }
        }
    });
}

void RCON::kick_all() {
    {
        std::lock_guard<std::mutex> lock(player_mutex);
        for (auto& _x : players) {
            this->send_command("kick " + _x.second.number + " Auto Kick!");
        }
    }
}

std::vector<RconPlayerInfo> RCON::get_players() {
    std::lock_guard<std::mutex> lock(player_mutex);

    std::vector<RconPlayerInfo> _players;

    _players.reserve(this->players.size());
    for (auto& _x : this->players) {
        _players.emplace_back(_x.second);
    }
    return _players;
}

void RCON::handle_disconnect() {

    loggedin = false;

}

void RCON::bind() {
    this->socket->bind_receive(
        [this](const std::string& received, size_t bytes_received) {
            this->handle_rec(received, bytes_received);
        }
    );
    this->connect();
}

bool RCON::is_logged_in() {
    return this->loggedin;
}

RCON::~RCON(void) {
    
    this->send_heart_beat = false;
    this->run_thread = false;

    if (this->worker.joinable()) {
        this->worker.join();
    }
}

void RCON::connect() {
    
    // Login Packet
    Packet rcon_packet;
    rcon_packet.command = password;
    rcon_packet.type = RCON::message_type::LOGIN;
    this->send_packet(rcon_packet);
    
}

void RCON::handle_rec(const std::string& msg, std::size_t bytes_received) {
    
    if (msg.size() >= 8) {
        switch (msg[7]) {
            case RCON::message_type::SERVER:{
                this->server_response(msg);
                break;
            }
            case RCON::message_type::CHAT:{
                this->chat_message(msg);
                break;
            }
            case RCON::message_type::LOGIN:{
                this->login_response(msg);
                break;
            }
            default : {
            }
        };
    }
}

void RCON::send_packet(const Packet& rcon_packet) {

    std::string cmd = this->make_be_message(rcon_packet.command,rcon_packet.type);

    this->socket->send(cmd,

    //handle sent
    [this](int _bytes) {
        this->handle_sent(_bytes);
    });
    
}

void RCON::send_packet(const Packet& rcon_packet, std::function<void(int)> _handle_sent) {

    std::string cmd = this->make_be_message(rcon_packet.command, rcon_packet.type);

    if (_handle_sent) {
        this->socket->send(cmd, _handle_sent);
    }
    else {

        this->socket->send(cmd,
            //handle sent
            [this](int _bytes) {
            this->handle_sent(_bytes);
            }
        );

    };

}

std::string RCON::make_be_message(const std::string& cmd, const message_type& _type) {

    std::string comand = {
        static_cast<char>(0xFFu),
        static_cast<char>(_type)
    };

    switch (_type) {
        case (RCON::message_type::SERVER): {
            comand += static_cast<char>(0x00);
            comand += cmd;
            current_seq_num = ++current_seq_num % 127;
            break;
        }
        case (RCON::message_type::CHAT): {
            comand += cmd[0];
            break;
        }
        case (RCON::message_type::LOGIN): {
            comand += cmd;
            break;
        }
    }
        
    auto _crc = CRC::Calculate(comand.data(), comand.size(), CRC::CRC_32());

    std::string packet = { 'B','E',

        static_cast<char>((_crc & 0x000000FF)),
        static_cast<char>((_crc & 0x0000FF00) >> 8),
        static_cast<char>((_crc & 0x00FF0000) >> 16),
        static_cast<char>((_crc & 0xFF000000) >> 24),

    };
    packet += comand;

    return packet;

}

void RCON::handle_sent(int _bytes_sent) {
    if (_bytes_sent < 0) {
        // Error
    }
}

void RCON::login_response(const std::string& _response) {

    if (_response[8] == 0x01) {
        this->loggedin = true;
        this->refresh_players();
    }
    else {
        this->loggedin = false;
    }

}

void RCON::server_response(const std::string& msg) {

    //7bit header + 1 msg type + 1 seq num
    //payload can be nothing, command or extraheader + command
    
    //nothing, just pure ACK for seq num
    if (msg.size() <= 9) {
        this->last_ACK = std::chrono::system_clock::now();
        return;
    }

    //ACK seq num
    unsigned char seq_num = msg[8];

    //extra header
    //0x00 | number of packets for this response | 0 - based index of the current packet
    bool extra_header = msg[9] == 0x00 && msg.size() > 9;   //the second part is just to make sure 

    //general
    //0x01 | received 1-byte sequence number | (possible header and/or response (ASCII string without null-terminator) OR nothing)

    if (!extra_header) {
        std::string result = msg.substr(9);
        this->process_message(seq_num, result);
    }
    else {
        
        //parse extra header
        unsigned char packets = msg[10];
        unsigned char packet = msg[11];

        std::string payload;
        if (msg.size() > 12) {
            payload = msg.substr(12);
        }

        bool _all_received = false;
        {
            std::lock_guard<std::mutex> lock(this->msg_cache_mutex);

            //          current num        parts
            std::pair< unsigned char, std::vector<std::string> > _cache;
            try {
                _cache = this->msg_cache.at(seq_num);
            }
            catch (std::out_of_range e) {
                _cache = {
                    0,
                    std::vector<std::string>(packets, "")
                };
            }

            if (_cache.second.size() != packets || _cache.second.size() == _cache.first) {
                //overwrite old entry
                _cache = {
                    0,
                    std::vector<std::string>(packets, "")
                };
            }

            _cache.first++;
            _cache.second[packet] = payload;

            if (_cache.first == packets) {
                _all_received = true;

                //rebuild msg
                payload.clear();
                for (auto& _part : _cache.second) {
                    payload += _part;
                }

                //we could erase the entry from the map here but.. well.. could cause rehash which takes longer

            }
            else {
                this->msg_cache.insert_or_assign(seq_num, _cache);
            }
        }

        if (_all_received) {
            this->process_message(seq_num, payload);
        }
    }
}

void RCON::chat_message(const std::string& msg) {
    
    // Respond to Server Msgs i.e chat messages, to prevent timeout
    Packet rcon_packet;
    rcon_packet.type = RCON::message_type::CHAT;
    rcon_packet.command = msg[8];
    this->send_packet(rcon_packet);

    // Received Chat Messages
    std::string result = msg.substr(9);
    
    //algorithm::trim(result);
    if (utils::starts_with(result, "Player #")) {

        if (utils::ends_with(result, " connected")) {
            result = result.substr(8);
            
            auto _space = result.find(" ");
            std::string player_number = result.substr(0, _space);
            
            auto _ip_bracket = result.find_last_of("(");
            std::string player_name = result.substr(_space + 1, _ip_bracket - (_space + 2));

            auto _ip_end = result.find(":");
            std::string ip = result.substr(_ip_bracket + 1, _ip_end - _ip_bracket - 1);
            int port = std::stoi(result.substr(_ip_end + 1, result.find(")", _ip_end) - (_ip_end + 1)));

            this->on_player_connect(player_number, player_name, ip, port);

        }
        else if (utils::ends_with(result, "disconnected")) {
            
            auto found = result.find(" ");
            std::string player_number = result.substr(0, found);

            auto found2 = result.find_last_of("(");
            std::string player_name = result.substr(found + 1, found2 - (found + 1));
            
            this->on_player_disconnect(player_number, player_name);
        }
    }
    else if (utils::starts_with(result, "Verified GUID")) {
        
        auto pos_1 = result.find("(");
        auto pos_2 = result.find(")", pos_1);

        std::string player_guid = result.substr((pos_1 + 1), (pos_2 - (pos_1 + 1)));

        pos_1 = result.find("#");
        pos_2 = result.find(" ", pos_1);
        std::string player_number = result.substr((pos_1 + 1), (pos_2 - (pos_1 + 1)));
        std::string player_name = result.substr(pos_2);

        this->on_player_verified_guid(player_number, player_name, player_guid);

    }
}

size_t RCON::get_player_count() {
    std::lock_guard<std::mutex> lock(this->player_mutex);
    return this->players.size();
}

void RCON::on_player_connect(const std::string& _player_number, const std::string& _player_name, const std::string& _ip, int _port) {

    RconPlayerInfo _info;
    {
        std::lock_guard<std::mutex> lock(this->player_mutex);
        try {
            _info = this->players.at(_player_name);
        }
        catch (std::out_of_range& e) {
            _info.player_name = _player_name;
        }
        _info.number = _player_number;
        _info.ip = _ip;
        _info.port = _port;
        this->players.insert_or_assign(_player_name, _info);
    }

    if (this->player_connected_callback) {
        this->player_connected_callback(_info);
    }
}

void RCON::on_player_disconnect(const std::string& _player_number, const std::string& _player_name) {
    
    RconPlayerInfo info;
    {
        std::lock_guard<std::mutex> lock(this->player_mutex);
        try {
            info = this->players.at(_player_name);
            this->players.erase(_player_name);
        } catch (...) {}
    }

    if (this->player_disconnected_callback) {
        this->player_disconnected_callback(info);
    }
}

void RCON::on_player_verified_guid(const std::string& _player_number, const std::string& _player_name, const std::string& _player_guid) {
    
    RconPlayerInfo _info;

    {
        std::lock_guard<std::mutex> lock(player_mutex);
        try {
            _info = this->players.at(_player_name);
            _info.guid = _player_guid;
            _info.verified = true;
        }
        catch (std::out_of_range& e) {
            _info.number = _player_number;
            _info.player_name = _player_name;
            _info.guid = _player_guid;
            _info.verified = true;
        }
        this->players.insert_or_assign(_player_name, _info);
    }

    if (this->player_verified_callback) {
        this->player_verified_callback(_info);
    }
}

void RCON::send_command(const std::string& command) {

    Packet rcon_packet;
    rcon_packet.command = command;
    remove_null_terminator(rcon_packet.command);
    rcon_packet.type = RCON::message_type::SERVER;

    this->send_packet(rcon_packet);
}

void RCON::send_global_msg(const std::string& msg) {
    this->send_command("say -1 " + msg);
}

void RCON::remove_ban(const std::string& uid) {
    
    Packet rcon_packet;
    rcon_packet.command = "removeBan " + uid;
    remove_null_terminator(rcon_packet.command);
    rcon_packet.type = RCON::message_type::SERVER;

    this->send_packet(rcon_packet, [this](int) {
        std::string cmd = "writeBans";
        remove_null_terminator(cmd);
        this->send_packet({ cmd, RCON::message_type::SERVER },
            [this](int) {
                this->send_command("loadBans");
            }
        );
    });
}

void RCON::add_ban(const std::string& uid) {
    this->add_ban(uid, "");
}

void RCON::add_ban(const std::string& uid, const std::string& reason, int duration) {
    
    Packet rcon_packet;
    rcon_packet.command = "addBan " + uid + " " + std::to_string(duration) + (reason.empty() ? "" : (" " + reason));
    remove_null_terminator(rcon_packet.command);
    rcon_packet.type = RCON::message_type::SERVER;

    this->send_packet(rcon_packet, [this](int) {
        std::string cmd = "writeBans";
        remove_null_terminator(cmd);
        this->send_packet({ cmd, RCON::message_type::SERVER },
            [this](int) {
                this->send_command("loadBans");
            }
        );
    });
}

void RCON::kick(const std::string& player_guid) {
    
    std::lock_guard<std::mutex> lock(this->player_mutex);
    for (auto& x : this->players) {
        if (x.second.guid == player_guid) {
            this->send_command("kick " + x.second.number);
            return;
        }
    }

}

void RCON::refresh_players() {

    this->send_command("players");

}

void RCON::process_message(unsigned char sequence_number, const std::string& message) {

    std::vector<std::string> tokens = utils::split(message, '\n');

    if (tokens.size() > 0) {
        if (tokens[0] == "Missions on server:") {
            this->process_message_missions(tokens);
        }
        else if (tokens[0] == "Players on server:") {
            this->process_message_players(tokens);
        }
        else if (tokens[0] == "GUID Bans:") {
            this->process_message_bans(tokens);
        }
        else {
            // Unknown
        }
    }

}

void RCON::process_message_players(const std::vector<std::string>& tokens) {

    //players on server:
    //table header
    //-------
    //player1
    //...
    //playerN
    //..total of XX players..

    for (size_t i = 3; i < (tokens.size() - 1); ++i) {

        std::string player_str  = tokens[i];
        
        auto _nr_end = player_str.find(" ");
        auto _ip_start = player_str.find_first_not_of(" ", _nr_end);
        auto _port_delim = player_str.find(":");
        auto _port_end = player_str.find(" ", _port_delim);
        auto _ping = player_str.find_first_not_of(" ",_port_end);
        auto _ping_end = player_str.find(" ", _ping);
        auto _guid = player_str.find_first_not_of(" ", _ping_end);
        auto _guid_end = player_str.find(" ", _guid);
        auto _name = player_str.find_first_not_of(" ", _guid_end);

        //check bad layout
        if (_ip_start == std::string::npos || _port_delim == std::string::npos || _ping == std::string::npos || _guid == std::string::npos || _name == std::string::npos) {
            continue;
        }

        RconPlayerInfo _info;
        _info.number = player_str.substr(0, _nr_end);
        _info.ip = player_str.substr(_ip_start, _port_delim - _ip_start);
        _info.port = std::stoi(player_str.substr(_port_delim + 1, _port_end - (_port_delim + 1)));
        _info.ping = std::stoi(player_str.substr(_ping, _ping_end - _ping));
        _info.guid = player_str.substr(_guid, _guid_end - _guid);
        _info.player_name = player_str.substr(_name);
        
        auto _veri_bracket = _info.guid.find("(");
        if (_veri_bracket != std::string::npos) {
            auto _veri_str = _info.guid.substr(_veri_bracket);
            if (_veri_str == "(OK)") {
                _info.verified = true;
            }
            _info.guid = _info.guid.substr(0, _veri_bracket);
        }

        //GET lobby
        _info.lobby = false;
        if (_info.player_name.find(" (Lobby)") != std::string::npos) {
            _info.player_name = _info.player_name.substr(0, _info.player_name.size() - 8);
            _info.lobby = true;
        }

        //insert into lookup
        {
            std::lock_guard<std::mutex> lock(this->player_mutex);

            try {
                auto _prev = this->players.at(_info.player_name);
                this->players.insert_or_assign(_info.player_name, _info);

                // check for missed verified notification
                if (_info.verified && !_prev.verified) {
                    this->on_player_verified_guid(_info.number, _info.player_name, _info.guid);
                }
            }
            catch (...) {
                // no previous --> just joined
                this->players.insert_or_assign(_info.player_name, _info);
                this->on_player_connect(_info.number, _info.player_name, _info.ip, _info.port);
                if (_info.verified) {
                    this->on_player_verified_guid(_info.number, _info.player_name, _info.guid);
                }
            }

        }
            
    }
}

void RCON::process_message_missions(const std::vector<std::string>& tokens) {

    std::vector<std::string> mission_names;
    for (size_t _i = 1; _i < tokens.size(); _i++) {
        auto& _x = tokens[_i];
        if (utils::ends_with(_x, ".pbo")) {
            mission_names.emplace_back(_x.substr(0, _x.size() - 4));
        }
        else {
            mission_names.emplace_back(_x);
        }
    }
    // TODO
}

void RCON::process_message_bans(const std::vector<std::string>& tokens) {

    /*
    GUID Bans:
    [#] [GUID] [Minutes left] [Reason]
    ----------------------------------------
    ...
    
    IP Bans:
    [#] [IP Address] [Minutes left] [Reason]
    ----------------------------------------------
    ...
    */

    bool _mode = false;
    for (auto& _x : tokens) {
        if (_x.size() == 0 || _x[0] == '-' || _x[0] == '[' || _x == "GUID Bans:") {
            //ignore
        }
        else if (_x == "IP Bans:") {
            _mode = true;
        }
        else {
            if (!_mode) {
                auto _guid_start = _x.find_first_not_of(" ",(_x.find(" ")));
                auto _length_start = _x.find_first_not_of(" ", (_x.find(" ",_guid_start)));
                auto _reason_start = _x.find_first_not_of(" ", (_x.find(" ",_length_start)));

                auto _guid = _x.substr(_guid_start, _length_start - _guid_start);
                auto _length = _x.substr(_length_start, _reason_start - _length_start);
                auto _reason = _x.substr(_reason_start);
            }
            else {
                auto _guid_start = _x.find_first_not_of(" ", (_x.find(" ")));
                auto _length_start = _x.find_first_not_of(" ", (_x.find(" ", _guid_start)));
                auto _reason_start = _x.find_first_not_of(" ", (_x.find(" ", _length_start)));

                auto _ip = _x.substr(_guid_start, _length_start - _guid_start);
                auto _length = _x.substr(_length_start, _reason_start - _length_start);
                auto _reason = _x.substr(_reason_start);
            }
        }
    }

}

void RCON::add_task(const RconTaskType& _type, const std::string& _data, bool _repeat, int _seconds, int _initdelay) {

    auto _new_task = std::make_shared<Task>();
    _new_task->data = _data;
    _new_task->type = _type;
    _new_task->repeat = _repeat;
    _new_task->seconds = _seconds;
    _new_task->exec_at = std::chrono::system_clock::now() + std::chrono::seconds(_seconds);

    this->insert_task(_new_task);
}

void RCON::insert_task(std::shared_ptr<Task> _new_task) {

    {
        std::lock_guard<std::mutex> lock(this->task_mutex);

        auto _cur = this->task_head;
        std::shared_ptr<Task> _before = nullptr;

        while (_cur) {
            if (_cur->exec_at >= _new_task->exec_at) {
                _before->next_task = _new_task;
                _new_task->next_task = _cur;
                break;
            }
            _before = _cur;
            _cur = _cur->next_task;
        }

        if (!_cur) {
            if (_before) {
                _before->next_task = _new_task;
            }
            else {
                this->task_head = _new_task;
            }
        }
    }

}

void RCON::enable_auto_reconnect() {
    this->auto_reconnect = true;
}

void RCON::lockServer() {
    this->send_command("#lock");
}

void RCON::unlockServer() {
    this->send_command("#unlock");
}

void RCON::shutdownServer() {
    this->send_command("#shutdown");
}

void RCON::set_player_connected_callback(const std::function<void(const RconPlayerInfo&)>& callback) {
    this->player_connected_callback = callback;
}
void RCON::set_player_disconnected_callback(const std::function<void(const RconPlayerInfo&)>& callback) {
    this->player_disconnected_callback = callback;
}
void RCON::set_player_verified_callback(const std::function<void(const RconPlayerInfo&)>& callback) {
    this->player_verified_callback = callback;
}
