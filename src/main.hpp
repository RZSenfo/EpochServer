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
typedef long long int int64;
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

#endif