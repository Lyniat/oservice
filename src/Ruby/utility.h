#pragma once
#include <string>
#include <cstdint>

#include "enet/enet.h"

std::string get_local_user_name();
uint64_t get_local_system_hash();
extern std::string get_local_network_ipv4();
