#ifndef __EPOCH_MAIN_HPP__
#define __EPOCH_MAIN_HPP__

#define EXTENSION_VERSION "0.1.0"

// TODO create Threadpool and Logger singleton

// GENERAL PURPOSE TYPE DEFS
typedef unsigned long long uint64;
typedef unsigned char uint8;

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

// THREADPOOL
#include <ThreadPool.hpp>

/*!< pointer to the threadpool (main.cpp) */
extern std::unique_ptr<ThreadPool> threadpool;

class EpochServer; /*!< Forward declare */
extern std::unique_ptr<EpochServer> server;


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

#ifdef WITH_INTERCEPT
    /**
    *   \brief Internal function wrapper for the parseSimpleArray recursion
    *
    *   Parses a string to an array (same restrictions as: https://community.bistudio.com/wiki/parseSimpleArray)
    *
    *   \param statement DBStatementOptions Options for the executed statement
    *   \param result std::shared_future<DBReturn> future to the result of the Database call
    *
    **/
    auto_array<game_value> parseSimpleArray(const std::string& in);
#endif
};

#endif