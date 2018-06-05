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

std::shared_ptr<Epochlib> EpochLibrary;

void split(const std::string &s, char delim, const std::shared_ptr<std::vector<std::string>>& elems) {
	std::stringstream ss(s);
	std::string item;
    elems->reserve(5);
	while (std::getline(ss, item, delim)) {
		elems->emplace_back(item);
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
    
    std::string infunc(_function);
    auto longStringDelim = infunc.find('#');
    if (longStringDelim != std::string::npos) {
        infunc = infunc.substr(0, longStringDelim);
    }

    std::shared_ptr<std::vector<std::string>> rawCmd = std::make_shared<std::vector<std::string>>(); 
    split(_function, SEPARATOR_CHAR, rawCmd);
	std::string hiveOutput = "";

    bool assignToInput = false;

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

		EpochLibrary = std::make_shared<Epochlib>(configPath, getProfileFolder(), _outputSize);
	}

	if (rawCmd->size() > 0) {
		
        std::string &callType = rawCmd->at(0);
        
        if (callType.size() == 3) {

            char callPrefix = callType[0];
            char functionType = callType[0];
            char asyncFlag = callType[2];

            switch (callPrefix) {
            case '0': {
                //SETUP
                if (asyncFlag == '0') {
                    //get config
                    hiveOutput = EpochLibrary->getConfig();
                }
                else {
                    //initial player check
                    if (rawCmd->size() >= 2) {
                        std::thread(
                        [rawCmd]() {
                            EpochLibrary->initPlayerCheck(
                                std::stoll(rawCmd->at(0))
                            );
                        }).detach();
                    }
                }
                break;
            }
            case '1': {
                // SET
                switch (functionType) {
                case '1': {
                    //SET
                    if (asyncFlag == '0') {
                        if (rawCmd->size() >= 4) {
                            hiveOutput = EpochLibrary->db->set(rawCmd->at(1), rawCmd->at(2), join(rawCmd, 3));
                        }
                    }
                    else {
                        if (rawCmd->size() >= 4) {
                            std::thread([rawCmd]() {
                                EpochLibrary->db->set(rawCmd->at(1), rawCmd->at(2), join(rawCmd, 3)); 
                            }).detach();
                        }
                    };
                    break;
                }
                case '2': {
                    //SETEX
                    if (asyncFlag == '0') {
                        if (rawCmd->size() >= 5) {
                            hiveOutput = EpochLibrary->db->setex(
                                rawCmd->at(1), 
                                rawCmd->at(2), 
                                rawCmd->at(3), 
                                join(rawCmd, 4)
                            );
                        }
                    }
                    else {
                        if (rawCmd->size() >= 5) {
                            std::thread(
                                [rawCmd]() {
                                    EpochLibrary->db->setex(
                                        rawCmd->at(1),
                                        rawCmd->at(2), 
                                        rawCmd->at(3), 
                                        join(rawCmd, 4)
                                    );
                                }
                            ).detach();
                        }
                    };
                    break;
                }
                case '3': {
                    //EXPIRE
                    if (asyncFlag == '0') {
                        if (rawCmd->size() >= 3) {
                            hiveOutput = EpochLibrary->db->expire(rawCmd->at(1), rawCmd->at(2));
                        }
                    }
                    else {
                        if (rawCmd->size() == 3) {
                            std::thread(
                                [rawCmd]() {
                                    EpochLibrary->db->expire(rawCmd->at(1), rawCmd->at(2));
                                }
                            ).detach();
                        }
                    };
                    break;
                }
                case '4': {
                    //SETBIT
                    if (rawCmd->size() >= 3) {
                        std::thread([rawCmd]() {
                            EpochLibrary->db->setbit(rawCmd->at(1), rawCmd->at(2), rawCmd->at(3));
                        }).detach();
                    }
                    break;
                }
                }

                break;
            }
            case '2': {
                //GET   
                switch (functionType) {
                case '0': {
                    //GET
                    assignToInput = true;
                    if (rawCmd->size() >= 2) {
                        hiveOutput = EpochLibrary->db->get(join(rawCmd, 1));
                    }
                    break;
                }
                case '1': {
                    //GET TTL
                    if (rawCmd->size() >= 2) {
                        hiveOutput = EpochLibrary->db->getTtl(join(rawCmd, 1));
                    }
                    break;
                }
                case '2': {
                    //GET RANGE
                    assignToInput = true;
                    if (rawCmd->size() >= 4) {
                        hiveOutput = EpochLibrary->db->getRange(rawCmd->at(1), rawCmd->at(2), rawCmd->at(3));
                    }
                    break;
                }
                case '4': {
                    //GET BIT
                    if (rawCmd->size() == 3) {
                        hiveOutput = EpochLibrary->db->getbit(rawCmd->at(1), rawCmd->at(2));
                    }
                    break;
                }
                case '5': {
                    //EXISTS
                    if (rawCmd->size() >= 2) {
                        hiveOutput = EpochLibrary->db->exists(join(rawCmd, 1));
                    }
                    break;
                }
                }
                break;
            }
            case '3': {
                //TTL
                if (rawCmd->size() >= 2) {
                    hiveOutput = EpochLibrary->db->ttl(join(rawCmd, 1));
                }
                break;
            }
            case '4': {
                //DEL
                if (rawCmd->size() >= 2) {
                    hiveOutput = EpochLibrary->db->del(join(rawCmd, 1));
                }
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
                if (rawCmd->size() >= 2) {
                    hiveOutput = EpochLibrary->db->lpopWithPrefix("CMD:", join(rawCmd, 1));
                }
                break;
            }
            case '7': {
                //LOGGING
                if (asyncFlag == '0') {
                    //sync
                    if (rawCmd->size() >= 3) {
                        hiveOutput = EpochLibrary->db->log(rawCmd->at(1), join(rawCmd, 2));
                    }
                }
                else {
                    // Async
                    if (rawCmd->size() >= 3) {
                        std::thread([rawCmd]() {
                            EpochLibrary->db->log(rawCmd->at(1), join(rawCmd, 2));
                        }).detach();
                    }
                }
                break;
            }
            case '8': {
                //ANTIHACK
                switch (functionType) {
                case '0': {
                    if (asyncFlag == '0') {
                        if (rawCmd->size() >= 2) {
                            rawCmd->erase(rawCmd->begin());
                            hiveOutput = EpochLibrary->updatePublicVariable(
                                *rawCmd
                            );
                        }
                    }
                    else {
                        if (rawCmd->size() >= 2) {
                            std::thread([rawCmd]() {
                                rawCmd->erase(rawCmd->begin());
                                EpochLibrary->updatePublicVariable(*rawCmd);
                            }).detach();
                        }
                    }
                    break;
                }
                case '1': {
                    if (rawCmd->size() == 2) {
                        hiveOutput = EpochLibrary->getRandomString(std::stoi(rawCmd->at(1)));
                    }
                    else if (rawCmd->size() == 1) {
                        hiveOutput = EpochLibrary->getRandomString(1);
                    }
                    break;
                }
                case '2': {
                    if (asyncFlag == '0') {
                        if (rawCmd->size() >= 3) {
                            hiveOutput = EpochLibrary->addBan(
                                std::stoll(rawCmd->at(1)), 
                                join(rawCmd, 2)
                            );
                        }
                    }
                    else {
                        if (rawCmd->size() >= 3) {
                            std::thread([rawCmd]() {
                                EpochLibrary->addBan(
                                    std::stoll(rawCmd->at(1)),
                                    join(rawCmd, 2)
                                ); 
                            }).detach();
                        }
                    }
                    break;
                }
                case '3': {
                    hiveOutput = EpochLibrary->db->increaseBancount();
                    break;
                }
                case '4': { // string to md5
                    if (rawCmd->size() >= 1) {
                        rawCmd->erase(rawCmd->begin());
                        hiveOutput = EpochLibrary->getStringMd5(*rawCmd);
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
                    if (rawCmd->size() > 1) {
                        std::thread([rawCmd]() {
                            EpochLibrary->beBroadcastMessage(rawCmd->at(1)); 
                        }).detach();
                    }
                    break;
                }
                case '1': { // kick (playerUID, Message)
                    if (rawCmd->size() > 2) {
                        std::thread([rawCmd]() {
                            EpochLibrary->beKick(rawCmd->at(1), rawCmd->at(2)); 
                        }).detach();
                    }
                    break;
                }
                case '2': { // ban  (playerUID, Message, Duration)
                    if (rawCmd->size() > 3) {
                        std::thread([rawCmd]() {
                            EpochLibrary->beBan(rawCmd->at(1), rawCmd->at(2), rawCmd->at(3));
                        }).detach();
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
            }
#endif
            default: {
                hiveOutput = "Unkown command " + rawCmd->at(0);
                //std::string OStext = std::to_string(_outputSize);
                //hiveOutput = "Unkown command " + rawCmd[0] + " Max hiveOutput " + OStext;
            }
            }
        }
        else {
            hiveOutput = "Unkown command " + rawCmd->at(0);
        }
	}
	else {
		hiveOutput = "0.6.0.0";
	}

    if (assignToInput) {

        if (hiveOutput.size() > 999500) {
            strncpy(_output, "[0,\"OUTPUT TOO LARGE\"]", _outputSize);
        }
        else {
            
            //get msg size and push it infront
            std::string size = std::to_string(hiveOutput.size());
            hiveOutput = size + "#" + hiveOutput;

            //overwrite input
            strncpy(_function, hiveOutput.c_str(), _outputSize);

            //output is empty by default
            //it signals that everthing is ok
        }

    }
    else {
	    strncpy(_output, hiveOutput.c_str(), _outputSize);
    }

	#ifdef __linux__
		_output[_outputSize - 1] = '\0';
		return;
	#endif
}
