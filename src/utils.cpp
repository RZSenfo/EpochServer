#include <main.hpp>

namespace utils {

    void trim_right(std::string &str) {
        size_t endpos = str.find_last_not_of(" \t");
        if (std::string::npos != endpos) {
            str = str.substr(0, endpos + 1);
        }
    }

    void trim_left(std::string &str) {
        size_t startpos = str.find_first_not_of(" \t");
        if (std::string::npos != startpos) {
            str = str.substr(startpos);
        }
    }

    std::string trim(std::string &str) {
        trim_left(str);
        trim_right(str);
        return str;
    }

    std::vector <std::string> split(const std::string &s, char delim) {
        std::vector <std::string> elems;
        std::string next;

        for (std::string::const_iterator it = s.begin(); it != s.end(); it++) {
            // If we've hit the terminal character
            if (*it == delim) {
                // If we have some characters accumulated
                if (!next.empty()) {
                    // Add them to the result vector
                    elems.push_back(next);
                    next.clear();
                }
            }
            else {
                // Accumulate the next character into the sequence
                next += *it;
            }
        }
        if (!next.empty())
            elems.push_back(next);
        return elems;

    }

    std::vector <std::string> split(const std::string &s, std::string &delim) {
        std::vector <std::string> elems;
        std::string next;

        if (delim.size() == 0) return elems;

        size_t _cur_pos = 0;
        for (std::string::const_iterator it = s.begin(); it != s.end(); it++) {
            // If we've hit the terminal character
            if (*it == delim.at(0) && _cur_pos < (s.size() - delim.size())) {

                std::string _cur_delim = s.substr(_cur_pos, delim.size());
                if (_cur_delim == delim) {
                    // If we have some characters accumulated
                    if (!next.empty()) {
                        // Add them to the result vector
                        elems.push_back(next);
                        next.clear();
                    }
                }
                else {
                    next += *it;
                }
            }
            else {
                // Accumulate the next character into the sequence
                next += *it;
            }
            _cur_pos++;
        }
        if (!next.empty()) {
            elems.push_back(next);
        }
        return elems;

    }

    bool starts_with(const std::string &s1, const std::string &s2) {
        return (s1.find(s2) == 0);
    }

    bool ends_with(const std::string &s1, const std::string &s2) {
        return (s1.find(s2) == s1.length() - s2.length());
    }

    bool iequals(const std::string& a, const std::string& b) {
        return std::equal(
            a.begin(), a.end(),
            b.begin(), b.end(),
            [](char a, char b) {
                return tolower(a) == tolower(b);
            }
        );
    }

    void replace_all(std::string &s1, const std::string &s2, const std::string &s3) {
        std::size_t iter = 0;
        while ((iter = s1.find(s2, iter)) != std::string::npos) {
            s1.replace(iter, s2.length(), s3);
        }
    }

    void remove_quotes(std::string &s) {
        if (s.size() > 0 && s.at(0) == '"') {
            s = s.substr(1, s.size() - 2);
        }
    }

    long long seconds_since(const utils::timestamp& _t) {
        return std::chrono::duration_cast<std::chrono::seconds>(sysclock::now() - _t).count();
    }

    long long minutes_since(const utils::timestamp& _t) {
        return std::chrono::duration_cast<std::chrono::minutes>(sysclock::now() - _t).count();
    }

    long long mili_seconds_since(const utils::timestamp& _t) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(sysclock::now() - _t).count();
    }

    template<typename T>
    T* pointerFromString(const std::string& _str) {
        std::stringstream _ss;
        _ss << _str;
        long long unsigned int _adr;
        _ss >> std::hex >> _adr;
        T * _data = reinterpret_cast<T *>(_adr);
        return _data;
    }

    template<typename T>
    std::string pointerToString(T* _ptr) {
        std::stringstream _ss;
        _ss << (void const *)_ptr;
        return _ss.str();
    }

#ifdef WITH_INTERCEPT
    auto_array<game_value> __parseSimpleArray(const std::string& in, size_t begin_idx, size_t& finished_index) {

        auto_array<game_value> out = {};

        // it has to be at least the opening bracket and the closing bracket
        if ((in.length() - begin_idx) >= 2 && *(in.begin() + begin_idx) == '[') {

            size_t current_index = 1;
            auto current = in.begin() + current_index;

            while (true) {

                if (*current == ']') {
                    // subarray end
                    finished_index = current_index + 1;
                    return out;
                }

                if (*current == ',') {
                    // next element
                    ++current_index; // after ,
                    current = in.begin() + current_index;
                }

                // cases: subarray, string, number, bool
                if (*current == '[') {
                    size_t done = 0;
                    out.emplace_back(__parseSimpleArray(in, current_index, done));

                    current_index = current_index + done;
                    current = in.begin() + current_index;

                }
                else if (*current == '"') {

                    // "" x "",
                    // find end
                    auto search_idx = current_index + 2;
                    auto idx = in.find('"', search_idx);

                    out.emplace_back(std::string(in.begin() + search_idx, in.begin() + idx));

                    current_index = idx + 2; // after ""
                    current = in.begin() + current_index;

                }
                else if ((*current >= '0' && *current <= '9') || *current == '.') {
                    auto idx = in.find_first_not_of("1234567890.",current_index) - 1;

                    out.emplace_back(std::stof(std::string(current, in.begin() + idx)));

                    current_index = idx;
                    current = in.begin() + current_index;
                }
                else if (*current == 't' || *current == 'f') {
                    out.emplace_back(*current == 't');
                    auto idx = in.find_first_not_of("truefals", current_index) - 1;
                    current_index = idx;
                    current = in.begin() + current_index;
                }
                else {
                    // unexpected char found
                    return {};
                }
            }
        }
        else {
            // malformed input: not an array
            return {};
        }
    }

    /**
    *   \brief Internal function wrapper for the parseSimpleArray recursion
    *
    *   Parses a string to an array (same restrictions as: https://community.bistudio.com/wiki/parseSimpleArray)
    *
    *   \param statement DBStatementOptions Options for the executed statement
    *   \param result std::shared_future<DBReturn> future to the result of the Database call
    *
    **/
    auto_array<game_value> parseSimpleArray(const std::string& in) {
        size_t idx = 0;
        return __parseSimpleArray(in, 0, idx);
    }
#endif
};