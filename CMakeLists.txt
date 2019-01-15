cmake_minimum_required(VERSION 3.6)

project(epochserver)

option(USE_ENGINE_TYPES "USE_ENGINE_TYPES" ON)

option(USE_64BIT_BUILD "USE_64BIT_BUILD" OFF)
set(INTERCEPT_LINK_TYPE "static")

if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
	set( USE_64BIT_BUILD ON)
endif()

# check for correct version of MSVC (c++17 compliant)
if("${CMAKE_GENERATOR}" MATCHES "Visual Studio 15 2017 Win64")
    set( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /std:c++latest" )
    set( USE_64BIT_BUILD ON) #since linux doesnt support 64 bit anyways
elseif("${CMAKE_GENERATOR}" MATCHES "Visual Studio 15 2017")
    set( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /std:c++latest" )
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    message(FATAL_ERROR "ERROR: You need a C++17 compatible compiler")
endif()

add_definitions(/D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS) #No I don't cheat!

message("GENERATOR USED: '${CMAKE_GENERATOR}'")
message("COMPILER USED: '${CMAKE_CXX_COMPILER_ID}'")

set(CMAKE_CL_64 ${USE_64BIT_BUILD})

if (USE_64BIT_BUILD)
    set(EXECUTABLE_NAME "epochserver_x64")
else()
    set(EXECUTABLE_NAME "epochserver")
endif()

if(USE_64BIT_BUILD)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/win64/")
elseif("${CMAKE_GENERATOR}" MATCHES "Visual Studio 15 2017")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/win32/")
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/linux/")
endif()

if(UNIX)
     set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

#yaml resets c++17 so I just move it above ours so we don't have to re-set
SET(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Don't want" FORCE)
SET(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Don't want" FORCE)
SET(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Don't want" FORCE)
SET(YAML_CPP_INSTALL OFF CACHE BOOL "Don't want" FORCE)
SET(MSVC_SHARED_RT OFF CACHE BOOL "want static" FORCE)
add_subdirectory( "${PROJECT_SOURCE_DIR}/deps/yaml-cpp" )

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_INCLUDE_CURRENT_DIR ON) 
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

SET(BUILD_STATIC ON CACHE BOOL "force static lib" FORCE)
SET(STATIC_MSVCRT ON CACHE BOOL "force static lib" FORCE)

SET(WITH_UNIT_TESTS OFF CACHE BOOL "no mysql tests" FORCE)
set(WITH_SSL "OFF" CACHE STRING "Disabled for now" FORCE)
#TODO test with wolfssl

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "-std=c++1z -O2 -s -fPIC -fpermissive -static-libgcc -static-libstdc++ -march=i686 -m32")
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
	set(CMAKE_SHARED_LINKER_FLAGS "-shared -static-libgcc -static-libstdc++")
else()
	set(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1 /MP /EHsc")
	set(CMAKE_CXX_FLAGS_RELEASE "/MT /Zi /O2 /Ob1 /EHsc /MP") #with debug info
	set(CMAKE_C_FLAGS_DEBUG "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1 /MP /EHsc")
	set(CMAKE_C_FLAGS_RELEASE "/MT /Zi /O2 /Ob1 /EHsc /MP") #with debug info
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/OPT:REF /DEBUG:FULL") 
endif()

add_subdirectory( "${PROJECT_SOURCE_DIR}/deps/mariadb-connector-c" )
add_subdirectory( "${PROJECT_SOURCE_DIR}/deps/mariadbpp" )

include_directories(AFTER "${PROJECT_SOURCE_DIR}/mariadbpp/include")
include_directories(AFTER "${PROJECT_SOURCE_DIR}/mariadb-connector-c/include")

add_subdirectory(src)


#glob external sources
file(GLOB_RECURSE SOURCES_HTTP "${PROJECT_SOURCE_DIR}/deps/happyhttp/happyhttp.cpp deps/happyhttp/*.h")

if (WIN32)
    SET(LIBS ${LIBS} ws2_32 hiredis win32_interop)
    if (USE_64BIT_BUILD)
        link_directories( "${PROJECT_SOURCE_DIR}/deps/hiredis/msvs/vs-solutions/vs2015/bin/x64/Release" )
    else()
        link_directories( "${PROJECT_SOURCE_DIR}/deps/hiredis/msvs/vs-solutions/vs2015/bin/x86/Release" )
    endif()

    include_directories( "${PROJECT_SOURCE_DIR}/deps/hiredis" )
else()
    SET(LIBS ${LIBS} hiredis)
    include_directories( "${PROJECT_SOURCE_DIR}/deps/hiredis-linux" )
    link_directories( "${PROJECT_SOURCE_DIR}/deps/hiredis-linux" )
endif()

include_directories( "${PROJECT_SOURCE_DIR}/deps/happyhttp" )

add_library (${EXECUTABLE_NAME} SHARED 
    ${SOURCES} 
    ${SOURCES_HTTP} 
)

add_subdirectory( "${PROJECT_SOURCE_DIR}/deps/pcre" )

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
