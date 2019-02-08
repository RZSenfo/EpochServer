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
    // (main.cpp)
    extern std::shared_ptr<spdlog::logger> logfile;
}

// GENERAL PURPOSE TYPE DEFS
typedef unsigned long long uint64;
typedef unsigned char uint8;

// THREADPOOL
#include <ThreadPool.hpp>

/*!< pointer to the threadpool (main.cpp) */
extern std::unique_ptr<ThreadPool> threadpool;

// INTERCEPT
#define WITH_INTERCEPT // TODO make it a CMAKE option
#ifdef WITH_INTERCEPT
    #include <intercept.hpp>
#endif

#include <string>
#include <vector>

// UTILS (utils.cpp)
namespace utils {
    void trim_right(std::string &str);

    void trim_left(std::string &str);

    std::string trim(std::string &str);

    std::vector <std::string> split(const std::string &s, char delim);

    std::vector <std::string> split(const std::string &s, std::string &delim);

    bool starts_with(const std::string &s1, const std::string &s2);

    bool ends_with(const std::string &s1, const std::string &s2);

    bool iequals(const std::string& a, const std::string& b);

    void replace_all(std::string &s1, const std::string &s2, const std::string &s3);

    void remove_quotes(std::string &s);

    typedef std::chrono::system_clock sysclock;
    typedef std::chrono::time_point<std::chrono::system_clock> timestamp;

    long long seconds_since(const utils::timestamp& _t);

    long long minutes_since(const utils::timestamp& _t);

    long long mili_seconds_since(const utils::timestamp& _t);

    template<typename T>
    T* pointerFromString(const std::string& _str);

    template<typename T>
    std::string pointerToString(T* _ptr);
};

#endif