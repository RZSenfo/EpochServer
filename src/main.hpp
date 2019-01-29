#ifndef __EPOCH_MAIN_HPP__
#define __EPOCH_MAIN_HPP__

// LOGGING
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/fmt/ostr.h>

#define INFO logging::logfile->info
#define DEBUG logging::logfile->debug
#define WARNING logging::logfile->warn

namespace logging {
    extern std::shared_ptr<spdlog::logger> logfile;
}

// GENERAL PURPOSE TYPE DEFS
typedef unsigned long long uint64;
typedef unsigned char uint8;

// THREADPOOL
#include <ThreadPool.hpp>

/*!< pointer to the threadpool */
std::unique_ptr<ThreadPool> threadpool;

// INTERCEPT
#define WITH_INTERCEPT // TODO make it a CMAKE option
#ifdef WITH_INTERCEPT
    #include <intercept.hpp>
#endif

// UTILS
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

    typedef std::chrono::system_clock sysclock;
    typedef std::chrono::time_point<std::chrono::system_clock> timestamp;
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
};

#endif