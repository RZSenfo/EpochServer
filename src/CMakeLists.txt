CMAKE_MINIMUM_REQUIRED( VERSION 3.0 )

OPTION( USE_ENGINE_TYPES "USE_ENGINE_TYPES" ON )
SET( INTERCEPT_LINK_TYPE "static" )

##############################################################################
##
## glob project sources
##
##############################################################################

INCLUDE_DIRECTORIES( public )

SET( PLUGIN_PUBLIC_PATH "${PROJECT_SOURCE_DIR}/src/public" )
SET( PLUGIN_PRIVATE_PATH "${PROJECT_SOURCE_DIR}/src/private" )

FILE( GLOB PLUGIN_main_SOURCES "${PROJECT_SOURCE_DIR}/src/*.hpp" "${PROJECT_SOURCE_DIR}/src/*.cpp" )
FILE( GLOB PLUGIN_BattlEye_SOURCES "${PLUGIN_PUBLIC_PATH}/BattlEye/*.hpp" "${PLUGIN_PRIVATE_PATH}/BattlEye/*.cpp" )
FILE( GLOB PLUGIN_SteamAPI_SOURCES "${PLUGIN_PUBLIC_PATH}/SteamAPI/*.hpp" "${PLUGIN_PRIVATE_PATH}/SteamAPI/*.cpp" )
FILE( GLOB PLUGIN_external_SOURCES "${PLUGIN_PUBLIC_PATH}/external/*.hpp" "${PLUGIN_PRIVATE_PATH}/external/*.cpp" )
FILE( GLOB PLUGIN_database_SOURCES "${PLUGIN_PUBLIC_PATH}/database/*.hpp" "${PLUGIN_PRIVATE_PATH}/database/*.cpp" )
FILE( GLOB PLUGIN_epochserver_SOURCES "${PLUGIN_PUBLIC_PATH}/epochserver/*.hpp" "${PLUGIN_PRIVATE_PATH}/epochserver/*.cpp" )

SOURCE_GROUP( "main" FILES ${PLUGIN_main_SOURCES} )
SOURCE_GROUP( "BattlEye" FILES ${PLUGIN_BattlEye_SOURCES} )
SOURCE_GROUP( "SteamAPI" FILES ${PLUGIN_SteamAPI_SOURCES} )
SOURCE_GROUP( "external" FILES ${PLUGIN_external_SOURCES} )
SOURCE_GROUP( "database" FILES ${PLUGIN_database_SOURCES} )
SOURCE_GROUP( "epochserver" FILES ${PLUGIN_epochserver_SOURCES} )

SET( INTERCEPT_PLUGIN_SOURCES
    ${PLUGIN_main_SOURCES}
    ${PLUGIN_BattlEye_SOURCES}
    ${PLUGIN_SteamAPI_SOURCES}
    ${PLUGIN_external_SOURCES}
    ${PLUGIN_database_SOURCES}
    ${PLUGIN_epochserver_SOURCES}
)

##############################################################################
##
## Intercept sources
## include the Intercept headers from the submodule
##
##############################################################################
SET( INTERCEPT_CLIENT_PATH "${CMAKE_SOURCE_DIR}/deps/intercept/src/client" )
SET( INTERCEPT_INCLUDE_PATH "${INTERCEPT_CLIENT_PATH}/headers" "${INTERCEPT_CLIENT_PATH}/headers/shared" "${INTERCEPT_CLIENT_PATH}/headers/client/" "${INTERCEPT_CLIENT_PATH}/headers/client/sqf" )

ADD_DEFINITIONS( /DINTERCEPT_NO_THREAD_SAFETY )

if(USE_ENGINE_TYPES)
    ADD_DEFINITIONS( /DINTERCEPT_SQF_STRTYPE_RSTRING )
endif()

FILE( GLOB INTERCEPT_HOST_SOURCES "${INTERCEPT_CLIENT_PATH}/intercept/client/*.cpp" "${INTERCEPT_CLIENT_PATH}/intercept/client/sqf/*.cpp" "${INTERCEPT_CLIENT_PATH}/intercept/shared/*.cpp" )
SOURCE_GROUP( "intercept" FILES ${INTERCEPT_HOST_SOURCES} )


##############################################################################
##
## Output name
##
##############################################################################
if(USE_64BIT_BUILD)
    SET( INTERCEPT_PLUGIN_NAME "${CMAKE_PROJECT_NAME}_x64" PARENT_SCOPE )
    SET( INTERCEPT_PLUGIN_NAME "${CMAKE_PROJECT_NAME}_x64" )
else()
    SET( INTERCEPT_PLUGIN_NAME "${CMAKE_PROJECT_NAME}" PARENT_SCOPE )
    SET( INTERCEPT_PLUGIN_NAME "${CMAKE_PROJECT_NAME}" )
endif()

##############################################################################
##
## "Baked in" Dependencies
##
##############################################################################

FILE( GLOB SOURCES_HTTP "${PROJECT_SOURCE_DIR}/deps/happyhttp/*p.cpp" "${PROJECT_SOURCE_DIR}/deps/happyhttp/*.h" )
SOURCE_GROUP( "src" FILES ${SOURCES_HTTP} )
INCLUDE_DIRECTORIES( "${CMAKE_SOURCE_DIR}/deps/happyhttp" )


##############################################################################
##
## specify the extension library
##
##############################################################################
INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_SOURCE_DIR} ${INTERCEPT_INCLUDE_PATH} )
ADD_LIBRARY( ${INTERCEPT_PLUGIN_NAME} SHARED 
	${INTERCEPT_PLUGIN_SOURCES}
	${INTERCEPT_HOST_SOURCES}	
    ${SOURCES_HTTP} 
)

##############################################################################
##
## specify hiredis location
##
##############################################################################
if(WIN32)
    # Windows
    SET( INTERCEPT_PLUGIN_LIBS  )
    if(USE_64BIT_BUILD)
        TARGET_LINK_LIBRARIES( ${INTERCEPT_PLUGIN_NAME} 
            debug "${PROJECT_SOURCE_DIR}/deps/hiredis/msvs/vs-solutions/vs2015/bin/x64/Debug/hiredis.lib"
            debug "${PROJECT_SOURCE_DIR}/deps/hiredis/msvs/vs-solutions/vs2015/bin/x64/Debug/win32_interop.lib"
            optimized "${PROJECT_SOURCE_DIR}/deps/hiredis/msvs/vs-solutions/vs2015/bin/x64/Release/hiredis.lib"
            optimized "${PROJECT_SOURCE_DIR}/deps/hiredis/msvs/vs-solutions/vs2015/bin/x64/Release/win32_interop.lib"
        )
        # LINK_DIRECTORIES( "${PROJECT_SOURCE_DIR}/deps/hiredis/msvs/vs-solutions/vs2015/bin/x64/Release" )
    else()
        TARGET_LINK_LIBRARIES( ${INTERCEPT_PLUGIN_NAME}
            debug "${PROJECT_SOURCE_DIR}/deps/hiredis/msvs/vs-solutions/vs2015/bin/x86/Debug/hiredis.lib"
            debug "${PROJECT_SOURCE_DIR}/deps/hiredis/msvs/vs-solutions/vs2015/bin/x86/Debug/win32_interop.lib"
            optimized "${PROJECT_SOURCE_DIR}/deps/hiredis/msvs/vs-solutions/vs2015/bin/x86/Release/hiredis.lib"
            optimized "${PROJECT_SOURCE_DIR}/deps/hiredis/msvs/vs-solutions/vs2015/bin/x86/Release/win32_interop.lib"
        )
        # LINK_DIRECTORIES( "${PROJECT_SOURCE_DIR}/deps/hiredis/msvs/vs-solutions/vs2015/bin/x86/Release" )
    endif()
    INCLUDE_DIRECTORIES( "${PROJECT_SOURCE_DIR}/deps/hiredis" )

    TARGET_LINK_LIBRARIES( ${INTERCEPT_PLUGIN_NAME} ws2_32 shlwapi )
    ADD_DEFINITIONS( -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN )
    SET_TARGET_PROPERTIES( ${INTERCEPT_PLUGIN_NAME} PROPERTIES LINK_FLAGS " /MANIFEST:NO /ERRORREPORT:NONE" )
else()
    # Linux
    INCLUDE_DIRECTORIES( "${PROJECT_SOURCE_DIR}/deps/hiredis-linux" )
    
    # built by cmake root file
    LINK_DIRECTORIES( "${PROJECT_SOURCE_DIR}/deps/hiredis-linux" )

    TARGET_LINK_LIBRARIES( ${INTERCEPT_PLUGIN_NAME} -Wl,-Bstatic hiredis -Wl,-Bdynamic )
    SET( CMAKE_CXX_FLAGS "-static-libstdc++ -static-libgcc ${CMAKE_CXX_FLAGS}" )
endif()

##############################################################################
##
## link general libs
##
##############################################################################
TARGET_LINK_LIBRARIES( ${INTERCEPT_PLUGIN_NAME}
    mariadbclientpp
    yaml-cpp
    pcre
    SQLiteCpp
)
INCLUDE_DIRECTORIES( "${PROJECT_SOURCE_DIR}/deps/spdlog/include" )
INCLUDE_DIRECTORIES( "${PROJECT_SOURCE_DIR}/deps/threadpool" )

##############################################################################
##
## library build properties
##
##############################################################################
SET_TARGET_PROPERTIES( ${INTERCEPT_PLUGIN_NAME} PROPERTIES PREFIX "" )
SET_TARGET_PROPERTIES( ${INTERCEPT_PLUGIN_NAME} PROPERTIES FOLDER "${CMAKE_PROJECT_NAME}" )

if(CMAKE_COMPILER_IS_GNUCXX)
	SET( CMAKE_CXX_FLAGS "-std=c++1z -O2 -s -fPIC -fpermissive -static-libgcc -static-libstdc++ -march=i686 -m32" )
	SET( CMAKE_FIND_LIBRARY_SUFFIXES ".a" )
	SET( CMAKE_SHARED_LINKER_FLAGS "-shared -static-libgcc -static-libstdc++" )
else()
	SET( CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1 /MP /EHsc" )
	SET( CMAKE_CXX_FLAGS_RELEASE "/MT /Zi /O2 /Ob1 /EHsc /MP" ) #with debug info
    SET( CMAKE_C_FLAGS_DEBUG "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1 /MP /EHsc" )
	SET( CMAKE_C_FLAGS_RELEASE "/MT /Zi /O2 /Ob1 /EHsc /MP" ) #with debug info
	SET( CMAKE_SHARED_LINKER_FLAGS_RELEASE "/OPT:REF /DEBUG:FULL" )
endif()