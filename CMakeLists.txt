cmake_minimum_required(VERSION 3.22)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(THIS_PROJECT_NAME "oservice")

project(${THIS_PROJECT_NAME} LANGUAGES C CXX)

option(UNET_MODULE_STEAM "UNET_MODULE_STEAM" OFF)

file(GLOB_RECURSE SRC_GLOB CONFIGURE_DEPENDS src/*.cpp)
list(REMOVE_ITEM SRC_GLOB ${CMAKE_CURRENT_SOURCE_DIR}/src/Services/ServiceGalaxy.cpp)

if(NOT UNET_MODULE_STEAM)
    list(REMOVE_ITEM SRC_GLOB ${CMAKE_CURRENT_SOURCE_DIR}/src/Services/ServiceSteam.cpp)
endif()

list(REMOVE_ITEM SRC_GLOB ${CMAKE_CURRENT_SOURCE_DIR}/src/xxhash.cpp)

if (WIN32)
    list(REMOVE_ITEM SRC_GLOB ${CMAKE_CURRENT_SOURCE_DIR}/src/System/SystemLinux.cpp)
elseif (APPLE)
    list(REMOVE_ITEM SRC_GLOB ${CMAKE_CURRENT_SOURCE_DIR}/src/System/SystemLinux.cpp)
    list(REMOVE_ITEM SRC_GLOB ${CMAKE_CURRENT_SOURCE_DIR}/src/System/SystemWindows.cpp)
    list(APPEND SRC_GLOB ${CMAKE_CURRENT_SOURCE_DIR}/src/System/SystemMacOS.cpp)
elseif(UNIX)
    list(REMOVE_ITEM SRC_GLOB ${CMAKE_CURRENT_SOURCE_DIR}/src/System/SystemMacOS.mm)
    list(REMOVE_ITEM SRC_GLOB ${CMAKE_CURRENT_SOURCE_DIR}/src/System/SystemWindows.cpp)
endif ()

include_directories(
        include
        third-party/dragonruby/include
)

if(UNET_MODULE_STEAM)
    include_directories(
            third-party/steam-sdk/public
    )
    link_directories(
            third-party/steam-sdk/redistributable_bin/win64
            third-party/steam-sdk/redistributable_bin/osx
            third-party/steam-sdk/redistributable_bin/linux64
    )
endif ()

add_compile_definitions(
        UNET_MODULE_ENET
        JSON_NOEXCEPTION
)

if(UNET_MODULE_STEAM)
    add_compile_definitions(
            UNET_MODULE_STEAM
    )
endif ()

if (WIN32)
    add_compile_definitions(
            GUID_WINDOWS
            PLATFORM_WINDOWS
    )
elseif (APPLE)
    add_compile_definitions(
            GUID_CFUUID
            PLATFORM_MACOS
    )
elseif(UNIX)
    add_compile_definitions(
            GUID_LIBUUID
            PLATFORM_LINUX
    )
    add_subdirectory(third-party/libuuid)
endif ()

add_library(${THIS_PROJECT_NAME} SHARED ${SRC_GLOB})

set_property(TARGET ${THIS_PROJECT_NAME} PROPERTY CXX_STANDARD 17)

set_target_properties(${THIS_PROJECT_NAME} PROPERTIES PREFIX "")

if (WIN32)
    set_target_properties(${THIS_PROJECT_NAME} PROPERTIES SUFFIX ".dll")
elseif (APPLE)
    set_target_properties(${THIS_PROJECT_NAME} PROPERTIES SUFFIX ".dylib")
elseif(UNIX)
    set_target_properties(${THIS_PROJECT_NAME} PROPERTIES SUFFIX ".so")
endif ()

set_target_properties(${THIS_PROJECT_NAME} PROPERTIES OUTPUT_NAME "oservice")

add_compile_definitions(USE_DRGTK=ON)
set(BUILD_TESTING OFF CACHE BOOL "OService tests" FORCE)
include(FetchContent)
FetchContent_Declare(
        ossp
        GIT_REPOSITORY https://github.com/Lyniat/ossp
        GIT_TAG        964cb3a1e837c2446633193b5aab6c8435bcfe07)

FetchContent_MakeAvailable(ossp)

add_subdirectory(third-party/enet)
add_subdirectory(third-party/udp-discovery)

if(UNET_MODULE_STEAM)
    if (WIN32)
        set(STEAM_LIB "steam_api64")
    else()
        set(STEAM_LIB "steam_api")
    endif ()
else()
    set(STEAM_LIB "")
endif()

if (WIN32)
    target_link_libraries(${THIS_PROJECT_NAME}
            PRIVATE
            enet
            udp-discovery
            ossp
            #ws2_32
            #winmm
            shlwapi
            Iphlpapi # this one is for utility.cpp
            PUBLIC
            ${STEAM_LIB}
    )
elseif (APPLE)
    target_link_libraries(${THIS_PROJECT_NAME}
            PRIVATE
            "-framework CoreFoundation"
            "-framework IOKit" # this one is for utility.cpp
            enet
            udp-discovery
            ossp
            PUBLIC
            ${STEAM_LIB}
    )
elseif (UNIX)
    target_link_libraries(${THIS_PROJECT_NAME}
            PRIVATE
            enet
            extuuid
            udp-discovery
            ossp
            PUBLIC
            ${STEAM_LIB}
    )
endif ()

target_compile_definitions(${THIS_PROJECT_NAME} PUBLIC XXH_INLINE_ALL)

if (WIN32)
    if(MSVC)
        target_compile_definitions(${THIS_PROJECT_NAME} PUBLIC _WINSOCKAPI_)
    else()
        message(FATAL_ERROR "Only the MSVC compiler is supported on Windows!")
    endif()
elseif (APPLE)
    target_compile_options(${THIS_PROJECT_NAME} PUBLIC -fPIC -fno-rtti -fno-exceptions)
elseif (UNIX)
    target_compile_options(${THIS_PROJECT_NAME} PUBLIC -fPIC -fno-rtti -fno-exceptions)
endif ()

if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /MT")
    target_compile_options(${THIS_PROJECT_NAME} PRIVATE "/we4716")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    target_compile_options(${THIS_PROJECT_NAME} PRIVATE "-Werror=return-type")
endif()

# build type
if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
    add_compile_definitions(META_TYPE="Release")
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_compile_definitions(META_TYPE="Debug")
    add_compile_definitions(DEBUG)
else()
    add_compile_definitions(META_TYPE="Other")
endif()

# add some helpful information to library
# get the latest commit hash of the working branch git branch
execute_process(
        COMMAND git log -1 --format=%H
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE META_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# get git repo name
execute_process(
        COMMAND git remote get-url origin
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE META_GIT_REPO
        #OUTPUT_STRIP_TRAILING_WHITESPACE
)

# get git branch name
execute_process(
        COMMAND git branch --show-current
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE META_GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (WIN32)
    set(META_PLATFORM \"Windows\")
elseif (APPLE)
    set(META_PLATFORM \"macOS\")
elseif (UNIX)
    set(META_PLATFORM \"Linux\")
endif ()

add_compile_definitions(META_PLATFORM=${META_PLATFORM})
add_compile_definitions(META_GIT_BRANCH="${META_GIT_BRANCH}")
add_compile_definitions(META_GIT_HASH="${META_GIT_HASH}")
add_compile_definitions(META_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}")
add_compile_definitions(META_COMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}")
add_compile_definitions(META_HOST_PLATFORM="${CMAKE_HOST_SYSTEM}")

set(CMAKE_SKIP_RPATH ON)
