#ifndef __EPOCH_MAIN_HPP__
#define __EPOCH_MAIN_HPP__

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/fmt/ostr.h>

#define INFO logging::logfile->info
#define DEBUG logging::logfile->debug
#define WARNING logging::logfile->warn

typedef unsigned char uint8;
typedef long long int int64;

namespace logging {
    extern std::shared_ptr<spdlog::logger> logfile;
}

#endif