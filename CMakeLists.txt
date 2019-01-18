CMAKE_MINIMUM_REQUIRED( VERSION 3.6 )

PROJECT( epochserver )

# 64 Bit Option (Windows only)
OPTION( USE_64BIT_BUILD "USE_64BIT_BUILD" OFF )

##############################################################################
##
## Check generator type
##
##############################################################################

# check for correct version of MSVC (c++17 compliant)
if("${CMAKE_GENERATOR}" MATCHES "Visual Studio 15 2017 Win64")
    SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /std:c++latest" )
    SET( USE_64BIT_BUILD ON )
elseif("${CMAKE_GENERATOR}" MATCHES "Visual Studio 15 2017")
    SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /std:c++latest" )
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    MESSAGE( FATAL_ERROR "ERROR: You need a C++17 compatible compiler" )
endif()

##############################################################################
##
## Check 64 bit config (windows only)
##
##############################################################################

if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
	SET( USE_64BIT_BUILD ON )
endif()

if(UNIX)
    MESSAGE( STATUS "DETECTED LINUX BUILD. DISABLED 64 BIT" )
    SET( USE_64BIT_BUILD OFF )
endif()
SET( CMAKE_CL_64 ${USE_64BIT_BUILD} )

ADD_DEFINITIONS( /D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS )

##############################################################################
##
## print generator type
##
##############################################################################
MESSAGE( "GENERATOR USED: '${CMAKE_GENERATOR}'" )
MESSAGE( "COMPILER USED: '${CMAKE_CXX_COMPILER_ID}'" )

##############################################################################
##
## Config and build dependencies
##
##############################################################################


##############################################################################
##
## YAML_CPP
## yaml resets c++17 so I just move it above ours so we don't have to re-set
##
##############################################################################
SET( YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Don't want" FORCE )
SET( YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Don't want" FORCE )
SET( YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Don't want" FORCE )
SET( YAML_CPP_INSTALL OFF CACHE BOOL "Don't want" FORCE )
SET( MSVC_SHARED_RT OFF CACHE BOOL "want static" FORCE )
ADD_SUBDIRECTORY( "${PROJECT_SOURCE_DIR}/deps/yaml-cpp" )

##############################################################################
##
## General Project Build Flags and output dir
##
##############################################################################
if("${CMAKE_GENERATOR}" MATCHES "Visual Studio 15 2017 Win64")
    SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/win64/" )
elseif("${CMAKE_GENERATOR}" MATCHES "Visual Studio 15 2017")
    SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/win32/" )
else()
    SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/linux/" )
    SET( CMAKE_CXX_FLAGS "-m32 -march=i686 ${CMAKE_CXX_FLAGS} " )
    SET( CMAKE_C_FLAGS "-m32 -march=i686 ${CMAKE_C_FLAGS} " )
    SET( CMAKE_SHARED_LINKER_FLAGS "-m32 -march=i686 ${CMAKE_SHARED_LINKER_FLAGS} " )
endif()

if(UNIX)
    SET( CMAKE_FIND_LIBRARY_SUFFIXES ".a" )
endif()

SET( CMAKE_CXX_STANDARD 17 )
SET( CMAKE_CXX_STANDARD_REQUIRED ON )
SET( CMAKE_CXX_EXTENSIONS OFF )

SET( CMAKE_INCLUDE_CURRENT_DIR ON )
SET_PROPERTY( GLOBAL PROPERTY USE_FOLDERS ON )

SET( CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE )

if(CMAKE_COMPILER_IS_GNUCXX)
    SET( CMAKE_CXX_FLAGS "-std=c++1z -O2 -s -fPIC -fpermissive -static-libgcc -static-libstdc++ ${CMAKE_CXX_FLAGS}" )
    SET( CMAKE_SHARED_LINKER_FLAGS "-shared -static-libgcc -static-libstdc++ ${CMAKE_SHARED_LINKER_FLAGS}" )
else()
    SET( CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1 /MP /EHsc" )
    SET( CMAKE_CXX_FLAGS_RELEASE "/MT /Zi /O2 /Ob1 /EHsc /MP" ) #with debug info
    SET( CMAKE_C_FLAGS_DEBUG "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1 /MP /EHsc" )
    SET( CMAKE_C_FLAGS_RELEASE "/MT /Zi /O2 /Ob1 /EHsc /MP" ) #with debug info
    SET( CMAKE_SHARED_LINKER_FLAGS_RELEASE "/OPT:REF /DEBUG:FULL" )
endif()


##############################################################################
##
## MariaDB Connector & MariaDBPP
##
##############################################################################
SET( BUILD_STATIC ON CACHE BOOL "force static lib" FORCE )
SET( STATIC_MSVCRT ON CACHE BOOL "force static lib" FORCE )

SET( WITH_UNIT_TESTS OFF CACHE BOOL "no mysql tests" FORCE )
SET( WITH_SSL "OFF" CACHE STRING "Disabled for now" FORCE )
#TODO test with wolfssl

INCLUDE_DIRECTORIES( ${PROJECT_SOURCE_DIR}/deps/mariadbpp/include )
INCLUDE_DIRECTORIES( ${PROJECT_SOURCE_DIR}/deps/mariadb-connector-c/include )


##############################################################################
##
## Configure and build mariadb connector
##
##############################################################################

MESSAGE( STATUS "CONFIGURING MARIADB CONNECTOR...")
EXECUTE_PROCESS(
    COMMAND "cmake" . -G${CMAKE_GENERATOR} -DWITH_UNIT_TESTS=OFF -DWITH_SSL=OFF -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/deps/mariadb-connector-c"
    RESULT_VARIABLE CMD_RESULT
    OUTPUT_VARIABLE CMD_OUTPUT
    ERROR_VARIABLE CMD_ERROR  
)
MESSAGE( STATUS "CMD_RESULT:" ${CMD_RESULT})
MESSAGE( STATUS "CMD_ERROR:" ${CMD_ERROR})
MESSAGE( STATUS "CMD_OUTPUT:" ${CMD_OUTPUT})
MESSAGE( STATUS "CONFIGURING MARIADB CONNECTOR DONE")

MESSAGE( STATUS "BUILDING MARIADB CONNECTOR...")
EXECUTE_PROCESS(
    COMMAND "cmake" --build . --config Release
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/deps/mariadb-connector-c"
    RESULT_VARIABLE CMD_RESULT
    OUTPUT_VARIABLE CMD_OUTPUT
    ERROR_VARIABLE CMD_ERROR  
)
MESSAGE( STATUS "CMD_RESULT:" ${CMD_RESULT} )
MESSAGE( STATUS "CMD_ERROR:" ${CMD_ERROR} )
MESSAGE( STATUS "CMD_OUTPUT:" ${CMD_OUTPUT} )
MESSAGE( STATUS "BUILDING MARIADB CONNECTOR DONE" )

FIND_LIBRARY( MariaDBClient_LIBRARY
    NAMES mariadb mariadbclient mysqlclient
    PATH_SUFFIXES mariadb mysql
    HINTS   ${PROJECT_SOURCE_DIR}/deps/mariadb-connector-c/libmariadb
            ${PROJECT_SOURCE_DIR}/deps/mariadb-connector-c/libmariadb/Release
            ${PROJECT_SOURCE_DIR}/deps/mariadb-connector-c/libmariadb/Debug
)
SET( MariaDBClient_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/deps/mariadb-connector-c/include" )

##############################################################################
##
## Override maria db cmake file since we build the connector ourself
##
##############################################################################
CONFIGURE_FILE( 
    ${PROJECT_SOURCE_DIR}/deps/changes/mariadbpp-CMakeLists.txt
    ${PROJECT_SOURCE_DIR}/deps/mariadbpp/CMakeLists.txt
    COPYONLY
)

ADD_SUBDIRECTORY( "${PROJECT_SOURCE_DIR}/deps/mariadbpp" )

##############################################################################
##
## build hiredis on linux
##
##############################################################################

if(UNIX)
    MESSAGE( STATUS "BUILDING hiredis CONNECTOR...")
    EXECUTE_PROCESS(
        COMMAND "make" CFLAGS=-m32 LDFLAGS=-m32
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/deps/hiredis-linux"
        RESULT_VARIABLE CMD_RESULT
        OUTPUT_VARIABLE CMD_OUTPUT
        ERROR_VARIABLE CMD_ERROR  
    )
    MESSAGE( STATUS "CMD_RESULT:" ${CMD_RESULT} )
    MESSAGE( STATUS "CMD_ERROR:" ${CMD_ERROR} )
    MESSAGE( STATUS "CMD_OUTPUT:" ${CMD_OUTPUT} )
    MESSAGE( STATUS "BUILDING hiredis CONNECTOR DONE" )

    SET( CMAKE_CXX_FLAGS "-m32 -march=i686 ${CMAKE_CXX_FLAGS}" )
    SET( CMAKE_C_FLAGS "-m32 -march=i686 ${CMAKE_C_FLAGS}" )
    SET( CMAKE_SHARED_LINKER_FLAGS "-m32 -march=i686 ${CMAKE_SHARED_LINKER_FLAGS}" )
endif()

##############################################################################
##
## pcre
##
##############################################################################

SET( PCRE_STATIC_RUNTIME ON CACHE BOOL "" FORCE )
SET( PCRE_BUILD_TESTS OFF CACHE BOOL "" FORCE )
ADD_SUBDIRECTORY( "${PROJECT_SOURCE_DIR}/deps/pcre" )
INCLUDE_DIRECTORIES( ${PROJECT_SOURCE_DIR}/deps/pcre )

CONFIGURE_FILE(
    ${PROJECT_SOURCE_DIR}/deps/pcre/pcre.h.in
    ${PROJECT_SOURCE_DIR}/deps/pcre/pcre.h 
    COPYONLY
)

##############################################################################
##
## rapid json
##
##############################################################################

SET( RAPIDJSON_BUILD_DOC OFF CACHE BOOL "" FORCE )
SET( RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE )
SET( RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE )
ADD_SUBDIRECTORY( "${PROJECT_SOURCE_DIR}/deps/rapidjson" )
INCLUDE_DIRECTORIES( ${PROJECT_SOURCE_DIR}/deps/rapidjson/include )

##############################################################################
##
## sqlitecpp
##
##############################################################################

SET( SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE )
SET( SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE )
ADD_SUBDIRECTORY( "${PROJECT_SOURCE_DIR}/deps/sqlitecpp" )


##############################################################################
##
## now the actual project
##
##############################################################################

ADD_SUBDIRECTORY( src )

GET_TARGET_PROPERTY( TARGET_LIBRARIES ${INTERCEPT_PLUGIN_NAME} LINK_LIBRARIES )
