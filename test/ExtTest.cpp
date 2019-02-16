#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
	#include <windows.h>
	#include <tchar.h>

	#define EXPORTIT __declspec( dllexport )
	#define STDCALL __stdcall
#else

	#include <dlfcn.h>
	#define EXPORTIT
	#define STDCALL

#endif

#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>


typedef void (STDCALL *fnc)(char *output, int outputSize, const char *function, const char **args, int argCnt);
typedef void (STDCALL *RVExtensionVersion)(char *output, int outputSize);


int main() {
	
    std::cout << "Starting.." << std::endl;

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
	HINSTANCE hinstDLL = LoadLibrary("epochserver.dll");
#else
	void* hinstDLL = dlopen("./epochserver.so", RTLD_LAZY);
#endif

	if (hinstDLL == NULL) {
		std::cout << "Could not load the dynamic library." << std::endl;
		return 1;
	}

	std::cout << "Successfully loaded the dynamic library!" << std::endl;

    
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)

    RVExtensionVersion version = (RVExtensionVersion)GetProcAddress(hinstDLL, "_RVExtensionVersion@8");
    if (!version) {
        std::cout << "Could not locate the entry function!" << std::endl;
        return 1;
    }
#else
    fnc func = (fnc)dlsym(hinstDLL, "RVExtensionVersion");
    const char *dlsym_error = dlerror();
    if (dlsym_error) {
        std::cout << "Could not locate the entry function!" << std::endl;
        dlclose(hinstDLL);
        return 1;
    }
#endif

    char veroutput[8050];
    version(veroutput, 8000);


    std::cout << "Version:" << veroutput << std::endl;

	// resolve function address

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)

	fnc func = (fnc)GetProcAddress(hinstDLL, "_RVExtensionArgs@20");
	if (!func) {
		std::cout << "Could not locate the entry function!" << std::endl;
		return 1;
	}
#else
	fnc func = (fnc)dlsym(hinstDLL, "RVExtensionArgs");
	const char *dlsym_error = dlerror();
	if (dlsym_error) {
		std::cout << "Could not locate the entry function!" << std::endl;
		dlclose(hinstDLL);
		return 1;
	}
#endif

	while (true) {
		std::cout << "Enter the function (or 'end' to exit): ";
	
		std::string callfunction;
		std::getline(std::cin, callfunction);
		if (callfunction == "end") break;
	
		std::cout << "Enter the function args (empty to finish): ";

        std::vector<std::string> args_str;
		std::vector<const char *> args;

		while (true) {
			std::string arg;
			std::getline(std::cin, arg);
			
			if (arg.empty()) {
				break;
			} else {
                args_str.emplace_back(arg);
				args.emplace_back(args_str.back().c_str());
			}
		}

        std::string output(8000, ' ');
		func(output.data(), output.size(), callfunction.c_str(), args.data(), args.size());
		
		std::cout << "The extension returned: " << output.c_str() << std::endl << std::endl;

	};

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)

#else
	dlclose(hinstDLL);
#endif

return 0;
}