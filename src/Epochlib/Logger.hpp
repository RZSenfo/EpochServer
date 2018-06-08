#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <fstream>
#include <ctime>
#include <iomanip>
#include <string>

class Logger {
private:
    std::string logFileName;
    std::ofstream logFile;

public:
    Logger(const std::string& LogFile);
    ~Logger();
    void log(const std::string& Message);
};

#endif