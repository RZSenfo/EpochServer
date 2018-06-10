cmake_minimum_required(VERSION 3.0)

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

if(WIN32)
include_directories(deps/hiredis)
else()
find_package(hiredis REQUIRED)
endif()

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

find_package(RapidJSON REQUIRED)
include_directories(${RAPIDJSON_INCLUDE_DIRS})

find_path(PCRE_INCLUDE pcre.h)
include_directories(${PCRE_INCLUDE})
find_library(PCRE_LIB pcre)
find_library(PCRED_LIB pcred)
link_libraries( 
    opitmized ${PCRE_LIB}
    debug ${PCRED_LIB}
)

message("PCRE Include Path:")
message(${PCRE_INCLUDE})
message("PCRE Lib Path:")
message(${PCRE_LIB})
SET(LIBS ${LIBS} ${PCRE_LIB})
SET(LIBS_DEBUG ${LIBS_DEBUG} ${PCRED_LIB})

# MYSQL
find_path(MYSQL_INCLUDE mysql/mysql.h)
include_directories(${MYSQL_INCLUDE})
find_library(MYSQL_LIB mysqlclient)
link_libraries(${MYSQL_LIB})
# TODO find debug version

if(WIN32)
find_library(EAY_LIB libeay32)
link_libraries(${EAY_LIB})
# TODO find debug version

find_library(SSL_LIB ssleay32)
link_libraries(${SSL_LIB})
# TODO find debug version

find_library(Z_LIB zlib)
find_library(Z_LIBD zlibd)
link_libraries(
    optimized ${Z_LIB}
    debug ${Z_LIBD}    
)
endif()
message("MYSQL Include Path:")
message(${MYSQL_INCLUDE})
message("MYSQL Lib Path:")
message(${MYSQL_LIB})

if(WIN32)
SET(LIBS ${LIBS} ${MYSQL_LIB} ${EAY_LIB} ${SSL_LIB} ${Z_LIB})
SET(LIBS_DEBUG ${LIBS_DEBUG} ${MYSQL_LIB} ${EAY_LIB} ${SSL_LIB} ${Z_LIBD})
else()
SET(LIBS ${LIBS} ${MYSQL_LIB})
SET(LIBS_DEBUG ${LIBS_DEBUG} ${MYSQL_LIB})
endif()
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

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    set(CMAKE_CXX_FLAGS_DEBUG_INIT          "/Zi /MT /WX- /0d /Ob0 /EHsc /GS- /Oy- /Oi /Ot /Oy /arch:SSE2 /fp:precise /DNOMINMAX /D _DEBUG /D_WINDLL /wd4996 /Zc:sizedDealloc-")
    set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT         "/MT /WX- /O1 /Ob0 /EHsc /GS- /Oy  /Oi /Ot /Oy /arch:SSE2 /fp:precise /DNOMINMAX /D NDEBUG /wd4996 /Zc:sizedDealloc-")
    set(CMAKE_CXX_FLAGS_RELEASE_INIT            "/MT /WX- /O2 /Ob1 /EHsc /GS- /Oy  /Oi /Ot /Oy /arch:SSE2 /fp:precise /DNOMINMAX /D NDEBUG /wd4996 /Zc:sizedDealloc-")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "/Zi /MT /Zi  /O2 /Ob1 /EHsc /GS- /Oy- /Oi /Ot /Oy /arch:SSE2 /fp:precise /DNOMINMAX /D _DEBUG /wd4996 /Zc:sizedDealloc-")
else()
    set(CMAKE_CXX_FLAGS_DEBUG_INIT            "-march=i686 -msse2 -msse3 -shared -fPIC -m32 -O2 -fomit-frame-pointer")
    set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT       "-march=i686 -msse2 -msse3 -shared -fPIC -m32 -O2 -fomit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELEASE_INIT          "-march=i686 -msse2 -msse3 -shared -fPIC -m32 -O2 -fomit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT   "-march=i686 -msse2 -msse3 -shared -fPIC -m32 -O2 -fomit-frame-pointer")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(${WIN32})
    # Windows
    target_link_libraries(${EXECUTABLE_NAME} 
        optimized ${LIBS}
        debug ${LIBS_DEBUG}    
    )
    add_definitions(-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN)
    SET_TARGET_PROPERTIES(${EXECUTABLE_NAME} PROPERTIES LINK_FLAGS " /MANIFEST:NO /ERRORREPORT:NONE")
else()
    # Linux
    target_link_libraries(${EXECUTABLE_NAME} -Wl,-Bstatic ${LIBS} -Wl,-Bdynamic)
    set(CMAKE_CXX_FLAGS "-static-libstdc++ -static-libgcc ${CMAKE_CXX_FLAGS}")

    ADD_CUSTOM_COMMAND(
        TARGET ${EXECUTABLE_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rename libepochserver.so epochserver.so
    )

endif()

mark_as_advanced(CMAKE_INSTALL_PREFIX)
mark_as_advanced(CMAKE_CONFIGURATION_TYPES)
