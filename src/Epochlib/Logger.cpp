#include "Logger.hpp"

Logger::Logger(const std::string& _logFile) {
    this->logFileName = _logFile;
    logFile.open(this->logFileName, std::ios::app);
}

Logger::~Logger() {
    logFile.close();
}

void Logger::log(const std::string& _message) {
    
    if (logFile.good()) {
#ifdef __linux__
            char outstr[200];
            time_t t       = time (NULL);
            struct tm *tmp = localtime (&t);
            if ( tmp != NULL && strftime(outstr, sizeof(outstr), "%Y-%m-%d %H:%M:%S ", tmp) != 0 ) {
                logFile << outstr << _message << std::endl;
        }
#else
        std::time_t t = std::time(nullptr);
        std::tm tm = *std::localtime(&t);
        logFile << std::put_time(&tm, "%Y-%m-%d %H:%M:%S ") << _message << std::endl;
#endif
    }
}