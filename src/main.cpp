#ifdef WIN32
    #include <windows.h>
    #include <shellapi.h>    //get command line argsW

    EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#else
    #include <fcntl.h>
    #include <limits.h>
    #include <unistd.h>
    #include <string.h>
#endif
#include <sstream>
#include <fstream>
#include <vector>
#include <thread>
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include <main.hpp>
#include <epochserver/epochserver.hpp>

#ifdef WIN32
extern "C" {
    __declspec (dllexport) void __stdcall RVExtension(char *output, int outputSize, char *function);
    __declspec (dllexport) int __stdcall RVExtensionArgs(char *output, int outputSize, const char *function, const char **args, int argsCnt);
};
#else
extern "C" {
    void RVExtension (char* output, int outputSize, char* function);
    int RVExtensionArgs(char *output, int outputSize, const char *function, const char **args, int argsCnt);
};
#endif

static std::unique_ptr<EpochServer> server;

/*
    RVExtension (Extension main call)
*/
#ifdef WIN32
int __stdcall RVExtensionArgs(char *output, int outputSize, const char *function, const char **args, int argsCnt) {
#elif __linux__
int RVExtensionArgs(char *output, int outputSize, const char *function, const char **args, int argsCnt) {
#endif
    return server->callExtensionEntrypoint(output, outputSize, function, args, argsCnt);
}

/*
    Only alternative syntax
*/
#ifdef WIN32
void __stdcall RVExtension(char *_output, int _outputSize, char *_function) {
#elif __linux__
void RVExtension(char *_output, int _outputSize, char *_function) {
#endif
    strncpy_s(_output, _outputSize, "Unsupported", _TRUNCATE);
}

#ifdef WIN32
#include <Windows.h>
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        server = std::make_unique<EpochServer>();
        logging::logfile = std::shared_ptr<spdlog::logger>{};
        spdlog::set_level(spdlog::level::debug);
        logging::logfile = spdlog::rotating_logger_mt(
            "logfile",
            "logs/epochserver.log",
            1024000,
            3
        );
        logging::logfile->flush_on(spdlog::level::debug);
        spdlog::set_pattern("[%H:%M:%S]-{%l}- %v");
        break;
    }
    case DLL_THREAD_ATTACH: {
        break;
    }
    case DLL_THREAD_DETACH: {
        break;
    }
    case DLL_PROCESS_DETACH: {
        logging::logfile->flush();
        spdlog::drop_all();
        break;
    }
    }

    return TRUE;
}

#else

#include <dlfcn.h>
static void __attribute__((constructor)) dll_load(void) {
    server = std::make_unique<EpochServer>();
}

static void __attribute__((destructor)) dll_unload(void) {
}
#endif
