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

#ifdef WIN32
extern "C" {
	__declspec (dllexport) void __stdcall RVExtension(char *output, int outputSize, const char *function);
}
#else
extern "C" {
        void RVExtension (char* output, int outputSize, const char* function);
}
#endif

#undef bind

Epochlib *EpochLibrary;

void split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;

	while (std::getline(ss, item, delim)) {
		elems.emplace_back(item);
	}
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

std::string join(const std::vector<std::string>& split, int index) {
	std::stringstream joinedString;

    //fetch the only one without seperator before
    //removes n-1 checks + possibly better loop unrolling
    auto it = split.begin() + index;
    if (it != split.end()) {
        joinedString << *it;
        it++;
    }

	for (; it != split.end(); it++) {
		joinedString << SEPARATOR << *it;
	}

	return joinedString.str();
}

/*
	Handler
*/
void handler000(const std::vector<std::string>& _param, std::string& output) {
	output = EpochLibrary->getConfig();
}
void handler001(const std::vector<std::string>& _param, std::string&) {
	if (_param.size() >= 2) {
		std::thread(std::bind(&Epochlib::initPlayerCheck, EpochLibrary, atoll(_param[1].c_str()))).detach();
	}
}

void handler110(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() >= 4) {
		output = EpochLibrary->set(_param[1], _param[2], join(_param, 3));
	}
}

void handler111(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() >= 4) {
		std::thread(std::bind(&Epochlib::set, EpochLibrary, _param[1], _param[2], join(_param, 3))).detach();
	}
}

void handler120(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() >= 5) {
		output = EpochLibrary->setex(_param[1], _param[2], _param[3], join(_param,4));
	}
}

void handler121(const std::vector<std::string>& _param, std::string&) {
	if (_param.size() >= 5) {
		std::thread(std::bind(&Epochlib::setex, EpochLibrary, _param[1], _param[2], _param[3], join(_param, 4))).detach();
	}
}

void handler130(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() == 3) {
		output = EpochLibrary->expire(_param[1], _param[2]);
	}
}

void handler131(const std::vector<std::string>& _param, std::string&) {
	if (_param.size() == 3) {
		std::thread(std::bind(&Epochlib::expire, EpochLibrary, _param[1], _param[2])).detach();
	}
}

void handler141(const std::vector<std::string>& _param, std::string&) {
	if (_param.size() >= 3) {
		std::thread(std::bind(&Epochlib::setbit, EpochLibrary, _param[1], _param[2], _param[3])).detach();
	}
}

void handler200(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() >= 2) {
		output = EpochLibrary->get(join(_param, 1));
	}
}

void handler210(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() >= 2) {
		output = EpochLibrary->getTtl(join(_param, 1));
	}
}

// Get Range
void handler220(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() >= 4) {
		output = EpochLibrary->getRange(_param[1], _param[2], _param[3]);
	}
}

void handler240(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() == 3) {
		output = EpochLibrary->getbit(_param[1], _param[2]);
	}
}

void handler250(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() >= 2) {
		output = EpochLibrary->exists(join(_param, 1));
	}
}

void handler300(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() >= 2) {
		output = EpochLibrary->ttl(join(_param, 1));
	}
}

void handler400(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() >= 2) {
		output = EpochLibrary->del(join(_param, 1));
	}
}

void handler500(const std::vector<std::string>& _param, std::string& output) {
	output = EpochLibrary->ping();
}

void handler510(const std::vector<std::string>& _param, std::string& output) {
	output = EpochLibrary->getCurrentTime();
}

void handler600(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() >= 2) {
		output = EpochLibrary->lpopWithPrefix("CMD:", join(_param, 1));
	}
}

void handler700(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() >= 3) {
		output = EpochLibrary->log(_param[1], join(_param, 2));
	}
}

void handler701(const std::vector<std::string>& _param, std::string&) {
	if (_param.size() >= 3) {
		std::thread(std::bind(&Epochlib::log, EpochLibrary, _param[1], join(_param, 2))).detach();
	}
}

void handler800(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() >= 2) {
        output = EpochLibrary->updatePublicVariable(
            std::vector<std::string>(
                _param.begin() + 1,
                _param.end()
            )
        );
	}
}
void handler801(const std::vector<std::string>& _param, std::string&) {
	if (_param.size() >= 2) {
		std::thread(std::bind(&Epochlib::updatePublicVariable, 
            EpochLibrary, 
            std::vector<std::string>(_param.begin() + 1, _param.end()))
        ).detach();
	}
}

void handler810(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() == 1) {
		output = EpochLibrary->getRandomString(atoi(_param[1].c_str()));
	}
	else if(_param.size() == 0) {
		output = EpochLibrary->getRandomString(1);
	}
}

void handler820(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() >= 3) {
		output = EpochLibrary->addBan(atoll(_param[1].c_str()), join(_param, 2));
	}
}
void handler821(const std::vector<std::string>& _param, std::string&) {
	if (_param.size() >= 3) {
		std::thread(std::bind(&Epochlib::addBan, EpochLibrary, atoll(_param[1].c_str()), join(_param, 2))).detach();
	}
}

void handler830(const std::vector<std::string>& _param, std::string& output) {
	output = EpochLibrary->increaseBancount();
}

void handler840(const std::vector<std::string>& _param, std::string& output) {
	if (_param.size() >= 1) {
		output = EpochLibrary->getStringMd5(std::vector<std::string>(_param.begin()+1,_param.end()));
	}
}

// Battleye Integration

// say  (Message)
void handler901(const std::vector<std::string>& _param, std::string&) {
    if (_param.size() > 1) {
    	std::thread(std::bind(&Epochlib::beBroadcastMessage, EpochLibrary, _param[1])).detach();
	}
}

// kick (playerUID, Message)
void handler911(const std::vector<std::string>& _param, std::string&) {
    if (_param.size() > 2) {
    	std::thread(std::bind(&Epochlib::beKick, EpochLibrary, _param[1], _param[2])).detach();
	}
}

// ban  (playerUID, Message, Duration)
void handler921(const std::vector<std::string>& _param, std::string&) {
    if (_param.size() > 3) {
        std::thread (std::bind(&Epochlib::beBan, EpochLibrary, _param[1], _param[2], _param[3])).detach();
	}
}

// lock
void handler931(const std::vector<std::string>& _param, std::string&) {
	std::thread(std::bind(&Epochlib::beLock, EpochLibrary)).detach();
}

// unlock
void handler930(const std::vector<std::string>& _param, std::string&) {
	std::thread(std::bind(&Epochlib::beUnlock, EpochLibrary)).detach();
}

// shutdown
void handler991(const std::vector<std::string>& _param, std::string&) {
	std::thread(std::bind(&Epochlib::beShutdown, EpochLibrary)).detach();
}


#ifdef EPOCHLIB_TEST
std::string handlerT100(std::vector<std::string> _param) {
	return EpochLibrary->getServerMD5();
}
#endif

/*
	RVExtension (Extension main call)
*/
#ifdef WIN32
void __stdcall RVExtension(char *_output, int _outputSize, const char *_function) {
#elif __linux__
void RVExtension(char *_output, int _outputSize, const char *_function) {
#endif
    std::vector<std::string> rawCmd; 
    split(std::string(_function), SEPARATOR[0], rawCmd);
	std::string hiveOutput = "";

	if (EpochLibrary == NULL) {
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

		EpochLibrary = new Epochlib(configPath, getProfileFolder(), _outputSize);
	}

	if (rawCmd.size() > 0) {
		
        std::string &callType = rawCmd[0];
        
        if (callType.size() == 3) {

            char callPrefix = callType[0];
            char functionType = callType[0];
            char asyncFlag = callType[2];

            switch (callPrefix) {
            case '0': {
                //SETUP
                if (asyncFlag == '0') {
                    //get config
                    handler000(rawCmd, hiveOutput);
                }
                else {
                    //initial player check
                    handler001(rawCmd, hiveOutput);
                }
                break;
            }
            case '1': {
                // SET
                switch (functionType) {
                case '1': {
                    //SET
                    if (asyncFlag == '0') {
                        handler110(rawCmd, hiveOutput);
                    }
                    else {
                        handler111(rawCmd, hiveOutput);
                    };
                    break;
                }
                case '2': {
                    //SETEX
                    if (asyncFlag == '0') {
                        handler120(rawCmd, hiveOutput);
                    }
                    else {
                        handler121(rawCmd, hiveOutput);
                    };
                    break;
                }
                case '3': {
                    //EXPIRE
                    if (asyncFlag == '0') {
                        handler130(rawCmd, hiveOutput);
                    }
                    else {
                        handler131(rawCmd, hiveOutput);
                    };
                    break;
                }
                case '4': {
                    //SETBIT
                    handler141(rawCmd, hiveOutput);
                    break;
                }
                }

                break;
            }
            case '2': {
                //GET   
                switch (functionType) {
                case '0': {
                    handler200(rawCmd, hiveOutput);
                    break;
                }
                case '1': {
                    handler210(rawCmd, hiveOutput);
                    break;
                }
                case '2': {
                    handler220(rawCmd, hiveOutput);
                    break;
                }
                case '4': {
                    handler240(rawCmd, hiveOutput);
                    break;
                }
                }
                break;
            }
            case '3': {
                //TTL
                handler300(rawCmd, hiveOutput);
                break;
            }
            case '4': {
                //TTL
                handler400(rawCmd, hiveOutput);
                break;
            }
            case '5': {
                //UTIL
                if (functionType == '0') {
                    handler500(rawCmd, hiveOutput);
                }
                else {
                    handler510(rawCmd, hiveOutput);
                }
                break;
            }
            case '6': {
                //ARRAY
                handler600(rawCmd, hiveOutput);
                break;
            }
            case '7': {
                //LOGGING
                if (asyncFlag == '0') {
                    //sync
                    handler700(rawCmd, hiveOutput);
                }
                else {
                    // Async
                    handler701(rawCmd, hiveOutput);
                }
                break;
            }
            case '8': {
                //ANTIHACK
                switch (functionType) {
                case '0': {
                    if (asyncFlag == '0') {
                        handler800(rawCmd, hiveOutput);
                    }
                    else {
                        handler801(rawCmd, hiveOutput);
                    }
                    break;
                }
                case '1': {
                    handler810(rawCmd, hiveOutput);
                    break;
                }
                case '2': {
                    if (asyncFlag == '0') {
                        handler820(rawCmd, hiveOutput);
                    }
                    else {
                        handler821(rawCmd, hiveOutput);
                    }
                    break;
                }
                case '3': {
                    handler830(rawCmd, hiveOutput);
                    break;
                }
                case '4': { // string to md5
                    handler840(rawCmd, hiveOutput);
                    break;
                }
                }
                break;
            }
            case '9': {
                //BATTLEYE
                switch (functionType) {
                case '0': { // say  (Message)
                    handler901(rawCmd, hiveOutput);
                    break;
                }
                case '1': { // kick (playerUID, Message)
                    handler911(rawCmd, hiveOutput);
                    break;
                }
                case '2': { // ban  (playerUID, Message, Duration)
                    handler921(rawCmd, hiveOutput);
                    break;
                }
                case '3': {
                    if (asyncFlag == '0') {
                        // Unlock
                        handler930(rawCmd, hiveOutput);
                    }
                    else {
                        // Lock Server
                        handler931(rawCmd, hiveOutput);
                    }
                    break;
                }
                case '9': {
                    // shutdown
                    handler991(rawCmd, hiveOutput);
                    break;
                }
                }
                break;
            }
#ifdef EPOCHLIB_TEST
            case 'T': {
                handlerT100(rawCmd, hiveOutput);
            }
#endif
            default: {
                hiveOutput = "Unkown command " + rawCmd[0];
                //std::string OStext = std::to_string(_outputSize);
                //hiveOutput = "Unkown command " + rawCmd[0] + " Max Output " + OStext;
            }
            }
        }
        else {
            hiveOutput = "Unkown command " + rawCmd[0];
        }
	}
	else {
		hiveOutput = "0.6.0.0";
	}

	strncpy(_output, hiveOutput.c_str(), _outputSize);

	#ifdef __linux__
		_output[_outputSize - 1] = '\0';
		return;
	#endif
}
