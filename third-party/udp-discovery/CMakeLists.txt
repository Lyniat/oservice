cmake_minimum_required(VERSION 3.22)

project(udp-discovery-cpp)

set(LIB_SOURCES
	udp_discovery_ip_port.cpp
	udp_discovery_peer.cpp
	udp_discovery_protocol.cpp)

add_library(udp-discovery STATIC ${LIB_SOURCES})
set_property(TARGET udp-discovery PROPERTY CXX_STANDARD 98)

target_include_directories(udp-discovery PUBLIC include)

if(UNIX)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
	target_compile_options(udp-discovery PUBLIC -fPIC)
endif()
