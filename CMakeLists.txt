cmake_minimum_required(VERSION 3.6)

project(epochserver)

#check for correct version of MSVC
if("${CMAKE_GENERATOR}" MATCHES "Visual Studio 15 2017 Win64")
	set( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /std:c++latest" )
	set( USE_64BIT_BUILD ON) #since linux doesnt support 64 bit anyways
elseif("${CMAKE_GENERATOR}" MATCHES "Visual Studio 15 2017")
	set( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /std:c++latest" )
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
	message(FATAL_ERROR "ERROR: You need a C++17 compatible compiler")
endif()

add_definitions(/D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS) #No I don't cheat!

#glob all sources
file(GLOB_RECURSE SOURCES src/*.cpp src/*.hpp)

#glob external sources
file(GLOB_RECURSE SOURCES_HTTP deps/happyhttp/happyhttp.cpp deps/happyhttp/*.h)

if (WIN32)
	SET(LIBS ${LIBS} ws2_32 hiredis win32_interop)
	if (USE_64BIT_BUILD)
		link_directories( deps/hiredis/msvs/vs-solutions/vs2015/bin/x64/Release )
	else()
		link_directories( deps/hiredis/msvs/vs-solutions/vs2015/bin/x86/Release )
	endif()
endif()

#include external headers
include_directories(deps/hiredis)
include_directories(deps/happyhttp)


if (USE_64BIT_BUILD)
	set(EXECUTABLE_NAME "epochserver_x64")
else()
	set(EXECUTABLE_NAME "epochserver")
endif()

add_library (${EXECUTABLE_NAME} SHARED 
	${SOURCES} 
	${SOURCES_HTTP} 
)


# -----------------------
# START STATIC LIBRARIES
# -----------------------
if(${UNIX})
     set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

find_package(rapidjson REQUIRED)
include_directories(${RAPIDJSON_INCLUDE_DIRS})

find_path(PCRE_INCLUDE pcre.h)
include_directories(${PCRE_INCLUDE})
find_library(PCRE_LIB pcre)
link_libraries(${PCRE_LIB})

message("PCRE Include Path:")
message(${PCRE_INCLUDE})
message("PCRE Lib Path:")
message(${PCRE_LIB})
SET(LIBS ${LIBS} ${PCRE_LIB})

# MYSQL
find_path(MYSQL_INCLUDE mysql/mysql.h)
include_directories(${MYSQL_INCLUDE})
find_library(MYSQL_LIB mysqlclient)
link_libraries(${MYSQL_LIB})

find_library(EAY_LIB libeay32)
link_libraries(${EAY_LIB})

find_library(SSL_LIB ssleay32)
link_libraries(${SSL_LIB})

find_library(Z_LIB zlib)
link_libraries(${Z_LIB})

message("MYSQL Include Path:")
message(${MYSQL_INCLUDE})
message("MYSQL Lib Path:")
message(${MYSQL_LIB})
SET(LIBS ${LIBS} ${MYSQL_LIB} ${EAY_LIB} ${SSL_LIB} ${Z_LIB})


# -----------------------
# END STATIC LIBRARIES
# -----------------------


# -----------------------
# START DYNAMIC LIBRARIES
# -----------------------
if(${UNIX})
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".so")
endif()


# -----------------------
# END DYNAMIC LIBRARIES
# -----------------------

#
# Build and Link options
#

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
	set(CMAKE_CXX_FLAGS_DEBUG_INIT          "/Zi /MT /WX- /0d /Ob0 /EHsc /GS- /Oy- /Oi /Ot /Oy /arch:SSE2 /fp:precise /DNOMINMAX /D_DEBUG /D _WINDLL /Zc:sizedDealloc-")
	set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT         "/MT /WX- /O1 /Ob0 /EHsc /GS- /Oy  /Oi /Ot /Oy /arch:SSE2 /fp:precise /DNOMINMAX /D NDEBUG /Zc:sizedDealloc-")
	set(CMAKE_CXX_FLAGS_RELEASE_INIT            "/MT /WX- /O2 /Ob1 /EHsc /GS- /Oy  /Oi /Ot /Oy /arch:SSE2 /fp:precise /DNOMINMAX /D NDEBUG /Zc:sizedDealloc-")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "/Zi /MT /Zi  /O2 /Ob1 /EHsc /GS- /Oy- /Oi /Ot /Oy /arch:SSE2 /fp:precise /DNOMINMAX /D_DEBUG /Zc:sizedDealloc-")
	message("MSVC!")
else()
	set(CMAKE_CXX_FLAGS_DEBUG_INIT 			  "-march=i686 -msse2 -msse3 -shared -fPIC -m32 -O2 -fomit-frame-pointer")
	set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT       "-march=i686 -msse2 -msse3 -shared -fPIC -m32 -O2 -fomit-frame-pointer")
	set(CMAKE_CXX_FLAGS_RELEASE_INIT          "-march=i686 -msse2 -msse3 -shared -fPIC -m32 -O2 -fomit-frame-pointer")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT   "-march=i686 -msse2 -msse3 -shared -fPIC -m32 -O2 -fomit-frame-pointer")
endif()

if(${WIN32})
	# Windows
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996")
  	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996")
	target_link_libraries(${EXECUTABLE_NAME} ${LIBS})
	add_definitions(-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN)
	SET_TARGET_PROPERTIES(${EXECUTABLE_NAME} PROPERTIES LINK_FLAGS " /MANIFEST:NO /ERRORREPORT:NONE")
else()
	# Linux
	target_link_libraries(${EXECUTABLE_NAME} -Wl,-Bstatic ${LIBS} -Wl,-Bdynamic)
	set(CMAKE_CXX_FLAGS "-std=c++0x -static-libstdc++ -static-libgcc ${CMAKE_CXX_FLAGS}")

	ADD_CUSTOM_COMMAND(
		TARGET ${EXECUTABLE_NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E rename libepochserver.so epochserver.so
	)

endif()

mark_as_advanced(CMAKE_INSTALL_PREFIX)
mark_as_advanced(CMAKE_CONFIGURATION_TYPES)