#include "Epochlib/Epochlib.hpp"
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

#define SEPARATOR "|"
#define SEPARATOR_CHAR '|'

#define DEV_DEBUG


#ifdef WIN32
extern "C" {
	__declspec (dllexport) void __stdcall RVExtension(char *output, int outputSize, char *function);
}
#else
extern "C" {
        void RVExtension (char* output, int outputSize, char* function);
}
#endif

#undef bind

std::unique_ptr<Epochlib> EpochLibrary;

std::unordered_map<unsigned long, std::string> resultcache;
unsigned long current_id = 0;

bool split1(const std::string&s, const char& delim, std::string& param1) {

    param1 = s.substr(0, s.find(delim));

#ifdef DEV_DEBUG
    std::stringstream sstream;
    sstream << std::endl << "s: " << s << std::endl;
    sstream << "p1: " << param1 << std::endl;
    EpochLibrary->log(
        sstream.str()
    );
#endif


    return true;
}

bool split2(const std::string&s, const char& delim, std::string& param1, std::string& param2) {

    auto idx1 = s.find(delim);
    if (idx1 == std::string::npos) return false;
    param1 = s.substr(0, idx1);
    idx1++;
    if (s.size() >= idx1) {
        auto idx2 = s.find(delim, idx1);
        if (idx2 == std::string::npos)
            param2 = s.substr(idx1);
        else
            param2 = s.substr(idx1, idx2 - idx1);
    }

#ifdef DEV_DEBUG
    std::stringstream sstream;
    sstream << std::endl << "s: " << s << std::endl;
    sstream << "p1: " << param1 << std::endl;
    sstream << "p2: " << param2 << std::endl;
    EpochLibrary->log(
        sstream.str()
    );
#endif

    return true;
}

bool split3(const std::string&s, const char& delim, std::string& param1, std::string& param2, std::string& param3) {

    auto idx1 = s.find(delim);
    if (idx1 == std::string::npos) return false;
    idx1++;
    auto idx2 = s.find(delim, idx1);
    if (idx2 == std::string::npos) return false;
    idx2++;
    param1 = s.substr(0, idx1 - 1);
    param2 = s.substr(idx1, idx2 - 1 - idx1);
    if (s.size() >= idx2) {
        auto idx3 = s.find(delim, idx2);
        if (idx3 == std::string::npos)
            param3 = s.substr(idx2);
        else 
            param3 = s.substr(idx2, idx3 - idx2);
    }

#ifdef DEV_DEBUG
    std::stringstream sstream;
    sstream << std::endl << "s: " << s << std::endl;
    sstream << "p1: " << param1 << std::endl;
    sstream << "p2: " << param2 << std::endl;
    sstream << "p3: " << param3 << std::endl;
    EpochLibrary->log(
        sstream.str()
    );
#endif

    return true;
}

bool split4(const std::string&s, const char& delim, std::string& param1, std::string& param2, std::string& param3, std::string& param4) {

    auto idx1 = s.find(delim);
    if (idx1 == std::string::npos) return false;
    idx1++;
    auto idx2 = s.find(delim, idx1);
    if (idx2 == std::string::npos) return false;
    idx2++;
    auto idx3 = s.find(delim, idx2);
    if (idx3 == std::string::npos) return false;
    idx3++;
    param1 = s.substr(0,    idx1 - 1);
    param2 = s.substr(idx1, idx2 - 1 - idx1);
    param3 = s.substr(idx2, idx3 - 1 - idx2);
    if (s.size() >= idx3) {
        auto idx4 = s.find(delim, idx3);
        if (idx4 == std::string::npos)
            param4 = s.substr(idx3);
        else
            param4 = s.substr(idx3, idx4 - idx3);
    }

#ifdef DEV_DEBUG
    std::stringstream sstream;
    sstream << std::endl << "s: " << s << std::endl;
    sstream << "p1: " << param1 << std::endl;
    sstream << "p2: " << param2 << std::endl;
    sstream << "p3: " << param3 << std::endl;
    sstream << "p4: " << param4 << std::endl;
    EpochLibrary->log(
        sstream.str()
    );
#endif

    return true;
}

void split(const std::string &s, char delim, const std::shared_ptr<std::vector<std::string>>& elems) {
	std::stringstream ss(s);
	std::string item;
    elems->reserve(5);
	while (std::getline(ss, item, delim)) {
		elems->emplace_back(item);
	}
}

void makeStringFromLongString(const char *s, std::string& func) {
    int longStringDelim = -1;

    int i = 0;  //TODO maybe ptr arith
    for (; s[i] != 0x00; i++) {
        if (s[i] == '#') {
            longStringDelim = i;
            break;
        }
    }

    if (longStringDelim > -1) {
        func.assign(s, longStringDelim);
    }
    else {
        func = s;
    }
}

void split_fromLongString(const char *s, char delim, const std::shared_ptr<std::vector<std::string>>& elems) {

    std::string infunc;
    makeStringFromLongString(s, infunc);
    split(infunc, delim, elems);

}

#ifdef WIN32
#include <locale>
#include <codecvt>
std::string ws2s(const std::wstring& wstr) {
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;
    return converterX.to_bytes(wstr);
}
#endif


std::string getProfileFolder() {
	std::string profileFolder = "";
	int numCmdLineArgs = 0;
	std::vector<std::string> commandLine;

#ifdef WIN32

	LPCWSTR cmdLine = GetCommandLineW();
	LPWSTR *cmdLineArgs = CommandLineToArgvW(cmdLine, &numCmdLineArgs);

	commandLine.reserve(numCmdLineArgs);

	for (int i = 0; i < numCmdLineArgs; i++) {
		std::wstring args(cmdLineArgs[i]);
		std::string utf8 = ws2s(args);
		commandLine.push_back(utf8);
	}

#else
	std::stringstream cmdlinePath;
	cmdlinePath << "/proc/" << (int)getpid() << "/cmdline";
	int cmdlineFile = open(cmdlinePath.str().c_str(), O_RDONLY);

	char cmdLineArgs[PATH_MAX];
	if ((numCmdLineArgs = read(cmdlineFile, cmdLineArgs, PATH_MAX)) > 0) {
		std::string procCmdline;
		procCmdline.assign(cmdLineArgs, numCmdLineArgs - 1);
		commandLine = split(procCmdline, '\0');
	}
#endif

	for (std::vector<std::string>::iterator it = commandLine.begin(); it != commandLine.end(); it++) {
		std::string starter = "-profiles=";
		if (it->length() < starter.length()) {
			continue;
		}

		std::string compareMe = it->substr(0, starter.length());
		if (compareMe.compare(starter) != 0) {
			continue;
		}

		profileFolder = it->substr(compareMe.length());
	}

	return profileFolder;
}

std::string join(const std::shared_ptr<std::vector<std::string>>& split, int index) {
	std::stringstream joinedString;

    //fetch the only one without seperator before
    //removes n-1 checks + possibly better loop unrolling
    auto it = split->begin() + index;
    auto end = split->end();
    if (it != end) {
        joinedString << *it;
        it++;
    }

	for (; it != end; it++) {
		joinedString << SEPARATOR << *it;
	}

	return joinedString.str();
}


/*
	RVExtension (Extension main call)
*/
#ifdef WIN32
void __stdcall RVExtension(char *_output, int _outputSize, char *_function) {
#elif __linux__
void RVExtension(char *_output, int _outputSize, char *_function) {
#endif
    
    std::string hiveOutput = "";

    //bool assignToInput = false;

    bool functionLen = _function[0] != 0 && _function[1] != 0 && _function[2] != 0;
    bool functionWithParam = functionLen && _function[3] != 0;
#define functionParamsBegin _function + 4

#ifdef DEV_DEBUG
    std::stringstream sstream;
    sstream << std::endl << "function: ";
    if (functionLen) sstream << _function[0] << _function[1] << _function[2];
    else sstream << " <undef> ";
    sstream << std::endl;
    sstream << "Has 3 Chars: " << std::boolalpha << functionLen << std::endl;
    sstream << "Has 4 Chars: " << std::boolalpha << functionWithParam << std::endl;
    //sstream << "Param: " << functionParamsBegin << std::endl;
    EpochLibrary->log(
        sstream.str()
    );
#endif

	if (functionLen) {
		
        char callPrefix = _function[0];
        char functionType = _function[1];
        char asyncFlag = _function[2];


        switch (callPrefix) {
        case '0': {
            //SETUP
            if (asyncFlag == '0') {
                //get config
                hiveOutput = EpochLibrary->getConfig();
            }
            else {
                //initial player check
                if (functionWithParam) {

                    std::string param;
                    if (split1(functionParamsBegin,SEPARATOR_CHAR,param)) {
                        std::thread(
                        [param]() {
                            try {
                                EpochLibrary->initPlayerCheck(
                                    std::stoll(param)
                                );
                            }
                            catch (...) {
                                //TODO log?
                            }
                        }).detach();
                    }
                    else hiveOutput = SQF::RET_FAIL();
                }
            }
            break;
        }
        case '1': {
            // SET
            if (functionWithParam) {


                switch (functionType) {
                case '1': {
                    //SET
                    
                    std::string param1, param2;
                    if (split2(functionParamsBegin, SEPARATOR_CHAR, param1, param2)) {

#ifdef DEV_DEBUG
                        sstream << "Param1: " << param1 << std::endl;
                        sstream << "Param2: " << param2 << std::endl;
                        sstream << "Param3 size: " << param2.size() << std::endl;
                        EpochLibrary->log(
                            sstream.str()
                        );
#endif



                        if (asyncFlag == '0') {

                            hiveOutput = EpochLibrary->db->set(param1, param2);
                            
                        }
                        else {
                            std::thread([param1, param2]() {
                                EpochLibrary->db->set(param1, param2);
                            }).detach();
                        }
                    }
                    else hiveOutput = SQF::RET_FAIL();
                    break;
                }
                case '2': {
                    //SETEX
                    
                    std::string p1, p2, p3;
                    if (split3(functionParamsBegin,SEPARATOR_CHAR,p1,p2,p3)) {
                        if (asyncFlag == '0') {
                        
                            hiveOutput = EpochLibrary->db->setex(p1,p2,p3);
                        }
                        else {
                            std::thread(
                                [p1,p2,p3]() {
                                    EpochLibrary->db->setex(p1,p2,p3);
                                }
                            ).detach();
                        }
                    }
                    else hiveOutput = SQF::RET_FAIL();
                    break;
                }
                case '3': {
                    //EXPIRE
                    
                    std::string param1, param2;
                    if (split2(functionParamsBegin, SEPARATOR_CHAR, param1, param2)) {

                        if (asyncFlag == '0') {
                            hiveOutput = EpochLibrary->db->expire(param1, param2);
                        }
                        else {
                            std::thread(
                                [param1, param2]() {
                                    EpochLibrary->db->expire(param1, param2);
                                }
                            ).detach();
                        }
                    }
                    else hiveOutput = SQF::RET_FAIL();
                    break;
                }
                case '4': {
                    //SETBIT
                    std::string p1, p2, p3;
                    if (split3(functionParamsBegin, SEPARATOR_CHAR, p1, p2, p3)) {
                        std::thread([p1,p2,p3]() {
                            EpochLibrary->db->setbit(p1,p2,p3);
                        }).detach();
                    }
                    break;
                }
                }
            }
            break;
        }
        case '2': {
            
            if (functionWithParam) {

                //GET
                switch (functionType) {
                case '0': {
                    //GET
                    
                    std::string p1; //key
                    if (split1(functionParamsBegin,SEPARATOR_CHAR,p1)) {
                        hiveOutput = EpochLibrary->db->get(p1);
                    }
                    else {
                        hiveOutput = SQF::RET_FAIL();
                    }
                    break;
                }
                case '1': {
                    //GET TTL
                    std::string p1; //key
                    if (split1(functionParamsBegin, SEPARATOR_CHAR, p1)) {
                        hiveOutput = EpochLibrary->db->getTtl(p1);
                    }
                    else hiveOutput = SQF::RET_FAIL();
                    break;
                }
                case '9': {
                    std::string p1; //fetchkey
                    if (split1(functionParamsBegin, SEPARATOR_CHAR, p1)) {
                        
                        try {

                            unsigned long _key = std::stoul(p1);

                            std::string& output = resultcache.at(_key);
                            
                            if (output.size() > 10000) {
                                hiveOutput = output.substr(0, 10000);
                                output = output.erase(0,10000);
                            }
                            else {
                                hiveOutput = output;
                            }

                        }
                        catch (...) {
                            hiveOutput = SQF::RET_FAIL();
                        }

                    }
                    else hiveOutput = SQF::RET_FAIL();
                    break;
                }
                case '2': {
                    //GET RANGE
                    std::string p1,p2,p3; //key idxfrom idxto
                    if (split3(functionParamsBegin, SEPARATOR_CHAR, p1,p2,p3)) {
                        hiveOutput = EpochLibrary->db->getRange(p1, p2, p3);
                    }
                    else hiveOutput = SQF::RET_FAIL();
                    break;
                }
                case '4': {
                    //GET BIT
                    std::string p1, p2; //key, index
                    if (split2(functionParamsBegin,SEPARATOR_CHAR,p1,p2)) {
                        hiveOutput = EpochLibrary->db->getbit(p1, p2);
                    }
                    else hiveOutput = SQF::RET_FAIL();
                    break;
                }
                case '5': {
                    //EXISTS
                    std::string p1; //key
                    if (split1(functionParamsBegin, SEPARATOR_CHAR, p1)) {
                        hiveOutput = EpochLibrary->db->exists(p1);
                    }
                    else hiveOutput = SQF::RET_FAIL();
                    break;
                }
                }
            }
            else {
                hiveOutput = SQF::RET_FAIL();
            }
            break;
        }
        case '3': {
            //TTL
            std::string p1;
            if (split1(functionParamsBegin,SEPARATOR_CHAR,p1)) {
                hiveOutput = EpochLibrary->db->ttl(p1);
            }
            else hiveOutput = SQF::RET_FAIL();
            break;
        }
        case '4': {
            //DEL
            std::string p1;
            if (split1(functionParamsBegin, SEPARATOR_CHAR, p1)) {
                hiveOutput = EpochLibrary->db->del(p1);
            }
            else hiveOutput = SQF::RET_FAIL();
            break;
        }
        case '5': {
            //UTIL
            if (functionType == '0') {
                hiveOutput = EpochLibrary->db->ping();
            }
            else {
                hiveOutput = EpochLibrary->getCurrentTime();
            }
            break;
        }
        case '6': {
            //ARRAY
            std::string p1;
            if (split1(functionParamsBegin, SEPARATOR_CHAR, p1)) {
                hiveOutput = EpochLibrary->db->lpopWithPrefix("CMD:", p1);
            }
            else hiveOutput = SQF::RET_FAIL();
            break;
        }
        case '7': {
            //LOGGING
            std::string p1, p2;
            if (split2(functionParamsBegin, SEPARATOR_CHAR, p1, p2)) {

                if (asyncFlag == '0') {
                    //sync
                    hiveOutput = EpochLibrary->db->log(p1,p2);
                }
                else {
                    // Async
                    std::thread([p1,p2]() {
                        EpochLibrary->db->log(p1, p2);
                    }).detach();
                }
            }
            else hiveOutput = SQF::RET_FAIL();
            break;
        }
        case '8': {
            //ANTIHACK
            switch (functionType) {
            case '0': {
                if (functionWithParam) {
                    std::shared_ptr<std::vector<std::string>> rawCmd = std::make_shared<std::vector<std::string>>();
                    split(functionParamsBegin, SEPARATOR_CHAR, rawCmd);

                    if (rawCmd->size() >= 2) {
                        if (asyncFlag == '0') {
                            hiveOutput = EpochLibrary->updatePublicVariable(
                                *rawCmd
                            );
                        }
                        else {
                            std::thread([rawCmd]() {
                                EpochLibrary->updatePublicVariable(*rawCmd);
                            }).detach();
                        }
                    }
                    else hiveOutput = SQF::RET_FAIL();
                }
                break;
            }
            case '1': {
                
                //TODO remove shared ptr, no use here
                std::shared_ptr<std::vector<std::string>> rawCmd = std::make_shared<std::vector<std::string>>();
                split(_function, SEPARATOR_CHAR, rawCmd);

                if (rawCmd->size() == 2) {
                    hiveOutput = EpochLibrary->getRandomString(std::stoi(rawCmd->at(1)));
                }
                else if (rawCmd->size() == 1) {
                    hiveOutput = EpochLibrary->getRandomString(1);
                }
                break;
            }
            case '2': {
                
                if (functionWithParam) {
                    std::string p1,p2;
                    if (split2(functionParamsBegin, SEPARATOR_CHAR, p1, p2)) {
                        if (asyncFlag == '0') {
                            try {
                                hiveOutput = EpochLibrary->addBan(
                                    std::stoll(p1),
                                    p2
                                );
                            }
                            catch (...) {
                                //TODO log?
                            }
                        }
                        else {
                            std::thread([p1,p2]() {
                                try {
                                    EpochLibrary->addBan(
                                        std::stoll(p1),
                                        p2
                                    );
                                }
                                catch (...) {
                                    //TODO log?
                                }
                            }).detach();
                        }
                    }
                    else hiveOutput = SQF::RET_FAIL();
                }
                break;
            }
            case '3': {
                hiveOutput = EpochLibrary->db->increaseBancount();
                break;
            }
            case '4': { // string to md5
                if (functionWithParam) {
                    std::string p1;
                    if (split1(functionParamsBegin, SEPARATOR_CHAR, p1)) {
                        hiveOutput = EpochLibrary->getStringMd5({ p1 });
                    }
                    else hiveOutput = SQF::RET_FAIL();
                }
                break;
            }
            }
            break;
        }
        case '9': {
            //BATTLEYE
            switch (functionType) {
            case '0': { // say  (Message)
                if (functionWithParam) {
                    std::string p1;
                    if (split1(functionParamsBegin, SEPARATOR_CHAR, p1)) {
                        std::thread([p1]() {
                            EpochLibrary->beBroadcastMessage(p1);
                        }).detach();
                    }
                    else hiveOutput = SQF::RET_FAIL();
                }
                break;
            }
            case '1': { // kick (playerUID, Message)
                if (functionWithParam) {
                    std::string p1,p2;
                    if (split2(functionParamsBegin, SEPARATOR_CHAR, p1,p2)) {
                        std::thread([p1,p2]() {
                            EpochLibrary->beKick(p1,p2);
                        }).detach();
                    }
                    else hiveOutput = SQF::RET_FAIL();
                }
                break;
            }
            case '2': { // ban  (playerUID, Message, Duration)
                if (functionWithParam) {
                    std::string p1,p2,p3;
                    if (split3(functionParamsBegin, SEPARATOR_CHAR, p1,p2,p3)) {
                        std::thread([p1,p2,p3]() {
                            EpochLibrary->beBan(p1,p2,p3);
                        }).detach();
                    }
                    else hiveOutput = SQF::RET_FAIL();
                }
                break;
            }
            case '3': {

                if (asyncFlag == '0') {
                    // Unlock
                    std::thread([](){
                        EpochLibrary->beUnlock();
                    }).detach();
                }
                else {
                    // Lock Server
                    std::thread([](){ 
                        EpochLibrary->beLock(); 
                    }).detach();
                }
                break;
            }
            case '9': {
                // shutdown
                std::thread(
                []() { 
                    EpochLibrary->beShutdown(); 
                }).detach();
                break;
            }
            }
            break;
        }
#ifdef EPOCHLIB_TEST
        case 'T': {
            hiveOutput = EpochLibrary->getServerMD5();
            break;
        }
#endif
        default: {
            hiveOutput = "Unkown command " + std::string(_function);
            //std::string OStext = std::to_string(_outputSize);
            //hiveOutput = "Unkown command " + rawCmd[0] + " Max hiveOutput " + OStext;
        }
        }
	}
	else {
		hiveOutput = "0.6.0.0";
	}

#ifdef DEV_DEBUG
    std::stringstream outstream;
    outstream << std::endl << "hiveOutput: " << hiveOutput << std::endl;
    EpochLibrary->log(
        outstream.str()
    );
#endif

    if (hiveOutput.size() >= _outputSize) {
        
        if (current_id > 10000) current_id = 0;
        
        resultcache.insert_or_assign(current_id, hiveOutput);

        hiveOutput = "[2," + std::to_string(current_id) + "]";

        current_id++;
    }

	strncpy(_output, hiveOutput.c_str(), _outputSize);
    
	#ifdef __linux__
		_output[_outputSize - 1] = '\0';
		return;
	#endif
}




void init() {
    if (EpochLibrary == nullptr) {
        std::string configPath;

#ifdef WIN32

        // Get file path
        WCHAR DllPath[MAX_PATH];
        GetModuleFileName((HINSTANCE)&__ImageBase, DllPath, MAX_PATH);

        std::string filePath = ws2s(DllPath);

        // Get file folder and use it as config folder
        configPath = filePath.substr(0, filePath.find_last_of("\\/"));
#elif __linux__
        configPath = "@epochhive";
#endif

        EpochLibrary = std::make_unique<Epochlib>(configPath, getProfileFolder(), 10 * 1024);
    }
}

#ifdef WIN32
#include <Windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        init();
        resultcache.reserve(10000);
        break;
    };
    case DLL_THREAD_ATTACH: {
        break;
    };
    case DLL_THREAD_DETACH: {
        break;
    };
    case DLL_PROCESS_DETACH: {
        break;
    };
    }

    return TRUE;
}
#endif

#ifdef __GNUC__
#include <dlfcn.h>
//GNU C compilers
static void __attribute__((constructor)) dll_load(void) {
    init();
}

static void __attribute__((destructor)) dll_unload(void) {
    
}
#endif