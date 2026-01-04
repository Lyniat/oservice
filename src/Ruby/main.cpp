#include <dragonruby.h>
#include <string>
#include <fstream>
#include <enet/enet.h>

#include "socket.rb.h"
#include "exceptions.rb.h"
#include "ossp/ossp.h"
#if defined(UNET_MODULE_STEAM)
#include <steam/steam_api.h>
#include <steam/steam_api_flat.h>
#endif
#include <filesystem>
#include <Unet/Services/ServiceEnet.h>

#include "Unet.h"
#include <bytebuffer/ByteBuffer.h>
#include <ossp/api.h>
#include <ossp/help.h>
#include <ossp/serialize.h>
#include "cppregex.h"
#include "komihash.h"
#include "symbols.h"
#include "utility.h"
#include "print.h"

#if defined(UNET_MODULE_STEAM)
#ifndef STEAM_APP_ID
#error Pass STEAM_APP_ID
#endif
#endif

using namespace lyniat::ossp::serialize::bin;
using namespace lyniat::memory::buffer;

const std::string STEAM_CALL_JOIN = R"(\+connect_lobby\ +(\d+))";

std::string appIdStr;

mrb_state* update_state;

RClass* drb_gtk;
RClass* drb_runtime;
RClass* drb_console;
RClass* steam;

RClass* exception_byte_buffer;
RClass* exception_invalid_packet;
RClass* exception_invalid_visibility;
RClass* exception_invalid_service;
RClass* exception_serialization;
RClass* exception_deserialization;

static Unet::IContext* g_ctx = nullptr;
static Unet::LobbyListResult g_lastLobbyList;
static bool use_steam = true;
static bool use_enet = true;
static bool use_galaxy = false;
static bool g_steamEnabled = false;
static bool g_galaxyEnabled = false;
static bool g_enetEnabled = false;
std::string lobby_to_join;

std::string get_argv(mrb_state* state);

#include "Callbacks.h"

#if defined(UNET_MODULE_STEAM)
static void InitializeSteam() {
    bool open = false;
    std::ifstream app_file;
#if defined(PLATFORM_WINDOWS)
SetEnvironmentVariableA ("SteamAppId", STEAM_APP_ID);
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
setenv ("SteamAppId", STEAM_APP_ID, 1);
#endif

LOG_INFO ("Enabling Steam service for " + appIdStr+ ".");

g_steamEnabled= SteamAPI_Init();
    if (!g_steamEnabled) {
        LOG_ERROR("Failed to initialize Steam API!");
    }
}
#endif

#ifdef DEBUG
void printr_dbg(std::string str) {
    print(update_state, PRINT_LOG, str.c_str());
}
#else
void printr_dbg(const std::string& str) {
}
#endif

std::string get_argv(mrb_state* state) {
    auto args = mrb_load_string(state, "$gtk.argv");
    return mrb_str_to_cstr(state, args);
}

std::string epoch_string_ms() {
    auto time_since_epoch = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    auto time_str = std::to_string(time_since_epoch);
    return time_str;
}

std::string rnd_string_64() {
    auto time_since_epoch = std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1);
    auto time_str = std::to_string(time_since_epoch);
    auto hash = komihash(time_str.c_str(), time_str.length(), 0);
    return std::to_string(hash);
}

std::string rnd_string_n(int length) {
    return rnd_string_64().substr(0, length);
}

inline uint64_t pext_symbol_komihash(mrb_state* state, mrb_sym symbol) {
    const auto sym_str = mrb_sym_name(state, symbol);
    return komihash(sym_str, strlen(sym_str), 0);
}

void init_unet() {
    g_ctx = Unet::CreateContext();
    g_ctx->SetCallbacks(new RubyCallbacks);

    #if defined(UNET_MODULE_STEAM)
    if (use_steam) {
        LOG_INFO("Enabled module: Steam");
        InitializeSteam();

        if (g_steamEnabled) {
            g_ctx->EnableService(Unet::ServiceType::Steam);
        }
    }
    #endif

    if (use_enet) {
        Unet::ServiceEnet::SetLocalMacAddress(get_local_system_hash());
        Unet::ServiceEnet::SetLocalUsername(get_local_user_name());
        //enet_initialize();
        LOG_INFO("Enabled module: Enet");
        g_ctx->EnableService(Unet::ServiceType::Enet);
        g_enetEnabled = true;
    }
}

void register_ruby_calls(mrb_state* state, RClass* module) {
    mrb_define_module_function(state, module, "__update_service", {
                                   [](mrb_state* mrb, mrb_value self) {
                                       update_state = mrb;
                                       #if defined(UNET_MODULE_STEAM)
                                       if (g_steamEnabled) {
                                           SteamAPI_RunCallbacks();
                                       }
                                       #endif
                                       g_ctx->RunCallbacks();
                                       const int max_channels = 1;
                                       for (int i = 0; i < max_channels; ++i) {
                                           auto data = g_ctx->ReadMessage(i);
                                           while (data != nullptr) {

                                               auto buffer = ByteBuffer(data.get()->m_data, data.get()->m_size, false);
                                               buffer.Uncompress();

                                               auto result = OSSP::Deserialize(&buffer, mrb);

                                               if (!result) {
                                                   auto error = generate_OSSP_error_message(result.error());
                                                   std::cout << error << std::endl;
                                                   mrb_raise(mrb, E_RUNTIME_ERROR, error.c_str());
                                               }

                                               auto result_value = result.value<>();
                                               mrb_value deserialized_data = result_value;
                                               if (mrb_type(result_value) == MRB_TT_ARRAY) {
                                                   auto array_size = RARRAY_LEN(result_value);
                                                   if (array_size > 0) {
                                                       deserialized_data = RARRAY_PTR(result_value)[0];
                                                   }
                                               }

                                               auto mrb_data = mrb_hash_new_capa(mrb, 3);
                                               pext_hash_set(mrb, mrb_data, "data", deserialized_data);
                                               auto peer = mrb_hash_new_capa(mrb, 2);
                                               pext_hash_set(mrb, peer, "id",
                                                             std::to_string(data.get()->m_peer.ID));
                                               pext_hash_set(mrb, peer, "service",
                                                             GetServiceNameByType(data.get()->m_peer.Service));
                                               pext_hash_set(mrb, mrb_data, "peer", peer);
                                               pext_hash_set(mrb, mrb_data, "channel", data.get()->m_channel);
                                               push_to_updates(on_data_received, mrb_data);
                                               data = g_ctx->ReadMessage(i);
                                           }
                                       }

                                       auto m_array = mrb_ary_new_capa(mrb, value_list.size());
                                       for (int i = 0; i < value_list.size(); i++) {
                                           mrb_ary_set(mrb, m_array, i, value_list[i]);
                                       }
                                       value_list.clear();
                                       mrb_funcall(
                                           mrb, self, "__exec_callback", 1, m_array);
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_NONE());

    mrb_define_module_function(state, module, "direct_connect", {
                                   [](mrb_state* state, mrb_value self) {
                                       printr_dbg("Connecting to Enet!\n");
                                       char* ip_str;
                                       mrb_int port = enet_default_port;
                                       mrb_get_args(state, "z|i", &ip_str, &port);

                                       ENetAddress addr;
                                       enet_address_set_host(&addr, ip_str);
                                       addr.port = port;
                                       g_ctx->JoinLobby(Unet::ServiceID(Unet::ServiceType::Enet, *(uint64_t*)&addr));

                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(1));

    mrb_define_module_function(state, module, "create_lobby", {
                                   [](mrb_state* mrb, mrb_value self) {
                                       printr_dbg("LobbyCreating!\n");
                                       char* chat_str;
                                       mrb_int lobby_size;
                                       mrb_sym type;
                                       mrb_get_args(mrb, "zin", &chat_str, &lobby_size, &type);
                                       auto lobby_type = Unet::LobbyPrivacy::Private;
                                       if (type == os_private) {
                                           lobby_type = Unet::LobbyPrivacy::Private;
                                       } else if (type == os_friends) {
                                           lobby_type = Unet::LobbyPrivacy::FriendsOnly;
                                       } else if (type == os_public) {
                                           lobby_type = Unet::LobbyPrivacy::Public;
                                       } else {
                                           ERR_INV_VISIBILITY
                                           return mrb_nil_value();
                                       }
                                       g_ctx->CreateLobby(lobby_type, (int)lobby_size, chat_str);
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(3));

    mrb_define_module_function(state, module, "get_lobby_to_join", {
                                   [](mrb_state* state, mrb_value self) {
                                       if (lobby_to_join.empty()) {
                                           return mrb_nil_value();
                                       }
                                       return mrb_str_new_cstr(state, lobby_to_join.c_str());
                                   }
                               }, MRB_ARGS_REQ(0));

    mrb_define_module_function(state, module, "get_lobby_to_join!", {
                                   [](mrb_state* state, mrb_value self) {
                                       if (lobby_to_join.empty()) {
                                           return mrb_nil_value();
                                       }
                                       auto return_value = mrb_str_new_cstr(state, lobby_to_join.c_str());
                                       lobby_to_join.clear();
                                       return return_value;
                                   }
                               }, MRB_ARGS_REQ(0));

    mrb_define_module_function(state, module, "list_lobbies", {
                                   [](mrb_state* state, mrb_value self) {
                                       printr_dbg("Listing Lobbies!\n");
                                       Unet::LobbyListFilter filter;
                                       g_ctx->GetLobbyList(filter);
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_NONE());

    mrb_define_module_function(state, module, "join_lobby", {
                                   [](mrb_state* state, mrb_value self) {
                                       printr_dbg("Joining Lobby!\n");
                                       mrb_int lobby_num;
                                       mrb_get_args(state, "i", &lobby_num);
                                       if (lobby_num >= g_lastLobbyList.Lobbies.size()) {
                                           printr_dbg("Invalid lobby num!\n");
                                           return mrb_nil_value();
                                       }
                                       auto lobbyInfo = g_lastLobbyList.Lobbies[lobby_num];
                                       g_ctx->JoinLobby(lobbyInfo);
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "join_lobby_by_id", {
                                   [](mrb_state* state, mrb_value self) {
                                       printr_dbg("Joining Lobby by id!\n");
                                       char* lobby_id;
                                       mrb_get_args(state, "z", &lobby_id);
                                       Unet::LobbyInfo lobby_info;
                                       bool found = false;
                                       for (auto& lobby : g_lastLobbyList.Lobbies) {
                                           if (found) {
                                               break;
                                           }
                                           auto lobby_entry_points = lobby.EntryPoints;
                                           for (auto& lobby_entry_point : lobby_entry_points) {
                                               auto entry_point = lobby_entry_point.ID;
                                               // Changed this from std::string to char* by using "z" instead of "S"
                                               if (lobby_id == std::to_string(entry_point)) {
                                                   lobby_info = lobby;
                                                   found = true;
                                                   break;
                                               }
                                           }
                                       }
                                       if (found) {
                                           g_ctx->JoinLobby(lobby_info);
                                       }
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "leave_lobby", {
                                   [](mrb_state* state, mrb_value self) {
                                       printr_dbg("Leaving Lobby!\n");
                                       g_ctx->LeaveLobby();
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_NONE());

    mrb_define_module_function(state, module, "set_lobby_name", {
                                   [](mrb_state* state, mrb_value self) {
                                       printr_dbg("Setting lobby name!\n");
                                       char* lobby_str;
                                       mrb_get_args(state, "z", &lobby_str);
                                       auto current_lobby = g_ctx->CurrentLobby();
                                       if (current_lobby == nullptr) {
                                           LOG_ERROR("Not in a lobby");
                                           return mrb_nil_value();
                                       }

                                       current_lobby->SetName(lobby_str);
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "show_lobby_data", {
                                   [](mrb_state* state, mrb_value self) {
                                       auto current_lobby = g_ctx->CurrentLobby();
                                       if (current_lobby == nullptr) {
                                           LOG_ERROR("Not in a lobby. Use \"data <num>\" instead.");
                                           return mrb_nil_value();
                                       }

                                       auto lobbyData = current_lobby->m_data;
                                       auto m_array = mrb_ary_new_capa(state, lobbyData.size());
                                       for (int i = 0; i < lobbyData.size(); i++) {
                                           auto data_i = lobbyData[i];
                                           auto hash = mrb_hash_new_capa(update_state, 2);
                                           pext_hash_set(update_state, hash, data_i.Name.c_str(), data_i.Value.c_str());
                                           mrb_ary_set(update_state, m_array, i, hash);
                                       }

                                       return m_array;
                                   }
                               }, MRB_ARGS_NONE());

    mrb_define_module_function(state, module, "show_member_data", {
                                   [](mrb_state* state, mrb_value self) {
                                       mrb_int peer;
                                       mrb_get_args(state, "i", &peer);
                                       auto current_lobby = g_ctx->CurrentLobby();
                                       if (current_lobby == nullptr) {
                                           LOG_ERROR("Not in a lobby.");
                                           return mrb_nil_value();
                                       }

                                       auto member = current_lobby->GetMember(peer);
                                       if (member == nullptr) {
                                           LOG_ERROR("Couldn't find member for peer " + std::to_string(peer));
                                           return mrb_nil_value();
                                       }

                                       auto m_array = mrb_ary_new_capa(state, member->m_data.size());
                                       for (int i = 0; i < member->m_data.size(); i++) {
                                           auto data_i = member->m_data[i];
                                           auto hash = mrb_hash_new_capa(update_state, 2);
                                           pext_hash_set(update_state, hash, data_i.Name.c_str(), data_i.Value.c_str());
                                           mrb_ary_set(update_state, m_array, i, hash);
                                       }
                                       return m_array;
                                   }
                               }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "get_lobby_members", {
                                   [](mrb_state* state, mrb_value self) {
                                       printr_dbg("Getting lobby members!\n");
                                       auto current = g_ctx->CurrentLobby();
                                       if (current == nullptr) {
                                           LOG_ERROR("Not in a lobby.");
                                           return mrb_nil_value();
                                       }
                                       auto members = current->GetMembers();
                                       auto members_array = mrb_ary_new_capa(state, members.size());
                                       for (int m = 0; m < members.size(); m++) {
                                           auto member = members[m];
                                           auto member_guid = member->UnetGuid.str();
                                           auto member_hash = mrb_hash_new_capa(state, 6);
                                           pext_hash_set(state, member_hash, "guid", member_guid);
                                           pext_hash_set(state, member_hash, "peer", member->UnetPeer);
                                           pext_hash_set(state, member_hash, "name", member->Name);
                                           pext_hash_set(state, member_hash, "valid", member->Valid);
                                           pext_hash_set(state, member_hash, "ping", member->Ping);
                                           auto id_array = mrb_ary_new_capa(state, member->IDs.size());
                                           for (int i = 0; i < member->IDs.size(); i++) {
                                               auto id = member->IDs[i];
                                               auto id_hash = mrb_hash_new_capa(state, 3);
                                               pext_hash_set(state, id_hash, "service",
                                                             Unet::GetServiceNameByType(id.Service));
                                               pext_hash_set(state, id_hash, "id", std::to_string(id.ID));
                                               pext_hash_set(state, id_hash, "primary",
                                                             member->UnetPrimaryService == id.Service);
                                               mrb_ary_set(state, id_array, i, id_hash);
                                           }
                                           pext_hash_set(state, member_hash, "ids", id_array);
                                           mrb_ary_set(state, members_array, m, member_hash);
                                       }
                                       return members_array;
                                   }
                               }, MRB_ARGS_NONE());

    mrb_define_module_function(state, module, "send_to_host", {
                                   [](mrb_state* mrb, mrb_value self) {
                                       mrb_value data;
                                       mrb_int channel = 0;
                                       mrb_sym rel_type = os_reliable;
                                       mrb_get_args(mrb, "o|in", &data, &channel, &rel_type);
                                       auto current_lobby = g_ctx->CurrentLobby();
                                       if (current_lobby == nullptr) {
                                           LOG_ERROR("Not in a lobby.");
                                           return mrb_nil_value();
                                       }
                                       auto host = current_lobby->GetHostMember();
                                       if (host == nullptr) {
                                           LOG_ERROR("No host available (yet).");
                                           return mrb_nil_value();
                                       }

                                       auto buffer = ByteBuffer();
                                       OSSP::Serialize(&buffer, mrb, data);
                                       if (!buffer.Compress()) {
                                           //LOG_ERROR("Compression failed!");
                                           ERR_BIN_BUFF
                                           return mrb_nil_value();
                                       }

                                       auto type = Unet::PacketType::Reliable;
                                       if (rel_type == os_reliable) {
                                           type = Unet::PacketType::Reliable;
                                       } else if (rel_type == os_unreliable) {
                                           type = Unet::PacketType::Unreliable;
                                       } else {
                                           ERR_INV_PACKET
                                           return mrb_nil_value();
                                       }

                                       g_ctx->SendToHost((uint8_t*)buffer.Data(), buffer.Size(), type, channel);
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(2));

    mrb_define_module_function(state, module, "send_to", {
                                   [](mrb_state* mrb, mrb_value self) {
                                       if (!g_ctx->IsHosting()) {
                                           push_error("'send_to' is only available for host!", -1);
                                           return mrb_nil_value();
                                       }
                                       mrb_value data;
                                       mrb_int peer;
                                       mrb_int channel = 0;
                                       mrb_sym rel_type = os_reliable;
                                       mrb_get_args(mrb, "oi|in", &data, &peer, &channel, &rel_type);
                                       auto current_lobby = g_ctx->CurrentLobby();
                                       if (current_lobby == nullptr) {
                                           LOG_ERROR("Not in a lobby.");
                                           return mrb_nil_value();
                                       }

                                       auto member = g_ctx->CurrentLobby()->GetMember(peer);

                                       auto buffer = ByteBuffer();
                                       OSSP::Serialize(&buffer, mrb, data);
                                       if (!buffer.Compress()) {
                                           LOG_ERROR("Compression failed!");
                                           return mrb_nil_value();
                                       }

                                       auto type = Unet::PacketType::Reliable;
                                       if (rel_type == os_reliable) {
                                           type = Unet::PacketType::Reliable;
                                       } else if (rel_type == os_unreliable) {
                                           type = Unet::PacketType::Unreliable;
                                       } else {
                                           ERR_INV_PACKET
                                           return mrb_nil_value();
                                       }

                                       g_ctx->SendTo(member, (uint8_t*)buffer.Data(), buffer.Size(), type, channel);
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(2) | MRB_ARGS_OPT(2));

    mrb_define_module_function(state, module, "send_to_members", {
                                   [](mrb_state* mrb, mrb_value self) {
                                       if (!g_ctx->IsHosting()) {
                                           push_error("'send_to_members' is only available for host!", -1);
                                           return mrb_nil_value();
                                       }
                                       mrb_value data;
                                       mrb_int channel = 0;
                                       mrb_sym rel_type = os_reliable;
                                       mrb_get_args(mrb, "o|in", &data, &channel, &rel_type);
                                       auto current_lobby = g_ctx->CurrentLobby();
                                       if (current_lobby == nullptr) {
                                           LOG_ERROR("Not in a lobby.");
                                           return mrb_nil_value();
                                       }

                                       auto buffer = ByteBuffer();
                                       OSSP::Serialize(&buffer, mrb, data);
                                       if (!buffer.Compress()) {
                                           LOG_ERROR("Compression failed!");
                                           return mrb_nil_value();
                                       }

                                       auto type = Unet::PacketType::Reliable;
                                       if (rel_type == os_reliable) {
                                           type = Unet::PacketType::Reliable;
                                       } else if (rel_type == os_unreliable) {
                                           type = Unet::PacketType::Unreliable;
                                       } else {
                                           ERR_INV_PACKET
                                           return mrb_nil_value();
                                       }

                                       g_ctx->SendToAll((uint8_t*)buffer.Data(), buffer.Size(), type, channel);
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(2));

    mrb_define_module_function(state, module, "send_chat", {
                                   [](mrb_state* state, mrb_value self) {
                                       char* chat_str;
                                       mrb_get_args(state, "z", &chat_str);
                                       if (chat_str != nullptr) {
                                           g_ctx->SendChat(chat_str);
                                       }
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "set_lobby_data", {
                                   [](mrb_state* state, mrb_value self) {
                                       char* key_str;
                                       char* value_str;
                                       mrb_get_args(state, "zz", &key_str, &value_str);

                                       auto current_lobby = g_ctx->CurrentLobby();
                                       if (current_lobby == nullptr) {
                                           LOG_ERROR("Not in a lobby.");
                                           return mrb_nil_value();
                                       }

                                       if (!current_lobby->GetInfo().IsHosting) {
                                           LOG_ERROR("Lobby data can only be set by the host.");
                                           return mrb_nil_value();
                                       }

                                       current_lobby->SetData(key_str, value_str);
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(2));

    mrb_define_module_function(state, module, "remove_lobby_data", {
                                   [](mrb_state* state, mrb_value self) {
                                       char* rem_str;
                                       mrb_get_args(state, "z", &rem_str);
                                       auto current_lobby = g_ctx->CurrentLobby();
                                       if (current_lobby == nullptr) {
                                           LOG_ERROR("Not in a lobby.");
                                           return mrb_nil_value();;
                                       }

                                       if (!current_lobby->GetInfo().IsHosting) {
                                           LOG_ERROR("Lobby data can only be removed by the host.");
                                           return mrb_nil_value();;
                                       }

                                       current_lobby->RemoveData(rem_str);
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "kick_member", {
                                   [](mrb_state* state, mrb_value self) {
                                       mrb_int peer;
                                       mrb_get_args(state, "i", &peer);
                                       auto current_lobby = g_ctx->CurrentLobby();
                                       if (current_lobby == nullptr) {
                                           LOG_ERROR("Not in a lobby.");
                                           return mrb_nil_value();;
                                       }
                                       auto member = current_lobby->GetMember(peer);
                                       if (member == nullptr) {
                                           LOG_ERROR("Member not found by peer.");
                                           return mrb_nil_value();
                                       }
                                       if (member->UnetGuid != g_ctx->GetLocalGuid()) {
                                           g_ctx->KickMember(member);
                                       } else {
                                           LOG_ERROR("You can't kick yourself.");
                                       }
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "am_i_host?", {
                                   [](mrb_state* state, mrb_value self) {
                                       return mrb_bool_value(g_ctx->IsHosting());
                                   }
                               }, MRB_ARGS_REQ(0));

    mrb_define_module_function(state, module, "connected?", {
                                   [](mrb_state* state, mrb_value self) {
                                       auto current_lobby = g_ctx->CurrentLobby();
                                       if (current_lobby == nullptr) {
                                           return mrb_bool_value(false);
                                       }
                                       return mrb_bool_value(true);
                                   }
                               }, MRB_ARGS_REQ(0));

    mrb_define_module_function(state, module, "get_ping", {
                                   [](mrb_state* state, mrb_value self) {
                                       mrb_int peer = 0;
                                       mrb_get_args(state, "|i", &peer);
                                       auto current_lobby = g_ctx->CurrentLobby();
                                       if (current_lobby == nullptr) {
                                           LOG_ERROR("Not in a lobby.");
                                           return mrb_nil_value();
                                       }

                                       Unet::LobbyMember* member = nullptr;
                                       if (peer == 0) {
                                           member = current_lobby->GetHostMember();
                                       } else {
                                           member = current_lobby->GetMember(peer);
                                       }

                                       if (member == nullptr) {
                                           LOG_ERROR("Member not found by peer.");
                                           return mrb_nil_value();
                                       }
                                       auto ping = member->Ping;
                                       return mrb_int_value(state, ping);
                                   }
                               }, MRB_ARGS_REQ(0) | MRB_ARGS_OPT(1));

    mrb_define_module_function(state, module, "is_host?", {
                                   [](mrb_state* state, mrb_value self) {
                                       mrb_int peer;
                                       mrb_get_args(state, "i", &peer);
                                       auto current_lobby = g_ctx->CurrentLobby();
                                       if (current_lobby == nullptr) {
                                           LOG_ERROR("Not in a lobby.");
                                           return mrb_nil_value();;
                                       }
                                       auto member = current_lobby->GetMember(peer);
                                       if (member == nullptr) {
                                           LOG_ERROR("Member not found by peer.");
                                           return mrb_nil_value();
                                       }
                                       auto host = current_lobby->GetHostMember();
                                       if (host == nullptr) {
                                           LOG_ERROR("Host not found.");
                                           return mrb_nil_value();
                                       }
                                       if (member->UnetGuid == host->UnetGuid) {
                                           return mrb_bool_value(true);
                                       }
                                       return mrb_bool_value(false);
                                   }
                               }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "who_is_by_peer?", {
                                   [](mrb_state* state, mrb_value self) {
                                       mrb_int peer;
                                       mrb_get_args(state, "i", &peer);
                                       auto current_lobby = g_ctx->CurrentLobby();
                                       if (current_lobby == nullptr) {
                                           LOG_ERROR("Not in a lobby.");
                                           return mrb_nil_value();;
                                       }
                                       auto member = current_lobby->GetMember(peer);
                                       if (member == nullptr) {
                                           LOG_ERROR("Member not found by peer.");
                                           return mrb_nil_value();
                                       }

                                       auto member_guid = member->UnetGuid.str();
                                       auto member_hash = mrb_hash_new_capa(state, 6);
                                       pext_hash_set(state, member_hash, "guid", member_guid);
                                       pext_hash_set(state, member_hash, "peer", member->UnetPeer);
                                       pext_hash_set(state, member_hash, "name", member->Name);
                                       pext_hash_set(state, member_hash, "valid", member->Valid);
                                       pext_hash_set(state, member_hash, "ping", member->Ping);
                                       auto id_array = mrb_ary_new_capa(state, member->IDs.size());
                                       for (int i = 0; i < member->IDs.size(); i++) {
                                           auto id = member->IDs[i];
                                           auto id_hash = mrb_hash_new_capa(state, 3);
                                           pext_hash_set(state, id_hash, "service",
                                                         Unet::GetServiceNameByType(id.Service));
                                           pext_hash_set(state, id_hash, "id", std::to_string(id.ID));
                                           pext_hash_set(state, id_hash, "primary",
                                                         member->UnetPrimaryService == id.Service);
                                           mrb_ary_set(state, id_array, i, id_hash);
                                       }
                                       pext_hash_set(state, member_hash, "ids", id_array);
                                       return member_hash;
                                   }
                               }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "who_is_me?", {
                                   [](mrb_state* state, mrb_value self) {
                                       auto current_lobby = g_ctx->CurrentLobby();
                                       if (current_lobby == nullptr) {
                                           LOG_ERROR("Not in a lobby.");
                                           return mrb_nil_value();;
                                       }
                                       auto own_peer = g_ctx->GetLocalPeer();
                                       auto member = current_lobby->GetMember(own_peer);
                                       if (member == nullptr) {
                                           LOG_ERROR("Member not found by peer.");
                                           return mrb_nil_value();
                                       }

                                       auto member_guid = member->UnetGuid.str();
                                       auto member_hash = mrb_hash_new_capa(state, 6);
                                       pext_hash_set(state, member_hash, "guid", member_guid);
                                       pext_hash_set(state, member_hash, "peer", member->UnetPeer);
                                       pext_hash_set(state, member_hash, "name", member->Name);
                                       pext_hash_set(state, member_hash, "valid", member->Valid);
                                       pext_hash_set(state, member_hash, "ping", member->Ping);
                                       auto id_array = mrb_ary_new_capa(state, member->IDs.size());
                                       for (int i = 0; i < member->IDs.size(); i++) {
                                           auto id = member->IDs[i];
                                           auto id_hash = mrb_hash_new_capa(state, 3);
                                           pext_hash_set(state, id_hash, "service",
                                                         Unet::GetServiceNameByType(id.Service));
                                           pext_hash_set(state, id_hash, "id", std::to_string(id.ID));
                                           pext_hash_set(state, id_hash, "primary",
                                                         member->UnetPrimaryService == id.Service);
                                           mrb_ary_set(state, id_array, i, id_hash);
                                       }
                                       pext_hash_set(state, member_hash, "ids", id_array);
                                       return member_hash;
                                   }
                               }, MRB_ARGS_REQ(0));

    // see: https://partner.steamgames.com/doc/api/ISteamFriends#richpresencelocalization
    mrb_define_module_function(state, module, "set_presence", {
                                   [](mrb_state* state, mrb_value self) {
                                       char* token_0;
                                       char* token_1;
                                       mrb_get_args(state, "zz", &token_0, &token_1);
                                       #if defined(UNET_MODULE_STEAM)
                                       auto friends = SteamFriends();
                                       if (friends == nullptr) {
                                           return mrb_nil_value();
                                       }

                                       auto str_0 = std::string(token_0);
                                       auto str_1 = std::string(token_1);
                                       str_0.resize(k_cchMaxRichPresenceKeyLength);
                                       str_1.resize(k_cchMaxRichPresenceValueLength);

                                       SteamAPI_ISteamFriends_SetRichPresence(friends, str_0.c_str(), str_1.c_str());
                                       #endif
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(2));

    mrb_define_module_function(state, module, "get_achievement_state", {
                               [](mrb_state* state, mrb_value self) {
                                   char* achievement_api_name;
                                   mrb_get_args(state, "z", &achievement_api_name);
                                   #if defined(UNET_MODULE_STEAM)
                                   auto user_stats = SteamAPI_SteamUserStats();
                                   bool achieved;
                                   uint32_t time;
                                   auto success = SteamAPI_ISteamUserStats_GetAchievementAndUnlockTime(user_stats, achievement_api_name, &achieved, &time);
                                   if (success) {
                                       auto hash = mrb_hash_new_capa(state, 2);
                                       cext_hash_set_kstr(state, hash, "achieved", mrb_bool_value(achieved));
                                       if (achieved) {
                                           // If the return value is true, but the unlock time is zero, that means it was unlocked before Steam began tracking achievement unlock times (December 2009).
                                           if (time == 0) {
                                               cext_hash_set_kstr(state, hash, "time", mrb_nil_value());
                                           } else {
                                               auto time_class = mrb_class_get(state, "Time");
                                               auto class_value = mrb_obj_value(time_class);
                                               auto time_value = mrb_int_value(state, time);
                                               auto time_instance = mrb_funcall(state, class_value, "at", 1, time_value);
                                               cext_hash_set_kstr(state, hash, "time", time_instance);
                                           }
                                       } else {
                                           cext_hash_set_kstr(state, hash, "time", mrb_nil_value());
                                       }
                                       return hash;
                                   }
                                   #endif
                                   return mrb_nil_value();
                               }
                           }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "get_achievement_global_percentage", {
                           [](mrb_state* state, mrb_value self) {
                               char* achievement_api_name;
                               mrb_get_args(state, "z", &achievement_api_name);
                               #if defined(UNET_MODULE_STEAM)
                               auto user_stats = SteamAPI_SteamUserStats();
                               float percentage;
                               auto success = SteamAPI_ISteamUserStats_GetAchievementAchievedPercent(user_stats, achievement_api_name, &percentage);
                               if (success) {
                                   return mrb_float_value(state, percentage);
                               }
                               #endif
                               return mrb_nil_value();
                           }
                       }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "get_achievement_attributes", {
                       [](mrb_state* state, mrb_value self) {
                           char* achievement_api_name;
                           mrb_get_args(state, "z", &achievement_api_name);
                           #if defined(UNET_MODULE_STEAM)
                           auto user_stats = SteamAPI_SteamUserStats();
                           auto name = SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute(user_stats, achievement_api_name, "name");
                           auto desc = SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute(user_stats, achievement_api_name, "desc");
                           auto hidden = SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute(user_stats, achievement_api_name, "hidden");
                           if (strlen(name) == 0 || strlen(desc) == 0 || strlen(hidden) == 0) {
                               return mrb_nil_value();
                           }
                           auto hash = mrb_hash_new_capa(state, 3);
                           cext_hash_set_kstr(state, hash, "name", mrb_str_new_cstr(state, name));
                           cext_hash_set_kstr(state, hash, "description", mrb_str_new_cstr(state, name));
                           auto is_hidden = std::string(hidden) != "0";
                           cext_hash_set_kstr(state, hash, "hidden", mrb_bool_value(is_hidden));
                           return hash;
                           #endif
                           return mrb_nil_value();
                       }
                   }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "unlock_achievement", {
                       [](mrb_state* state, mrb_value self) {
                           char* achievement_api_name;
                           mrb_get_args(state, "z", &achievement_api_name);
                           #if defined(UNET_MODULE_STEAM)
                           auto user_stats = SteamAPI_SteamUserStats();
                           auto success = SteamAPI_ISteamUserStats_SetAchievement(user_stats, achievement_api_name);
                           if (success) {
                               return mrb_true_value();
                           }
                           #endif
                           return mrb_false_value();
                       }
                   }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "__dbg_clear_achievement", {
                   [](mrb_state* state, mrb_value self) {
                       return mrb_nil_value();
                       char* achievement_api_name;
                       mrb_get_args(state, "z", &achievement_api_name);
                       #if defined(UNET_MODULE_STEAM)
                       auto user_stats = SteamAPI_SteamUserStats();
                       auto success = SteamAPI_ISteamUserStats_ClearAchievement(user_stats, achievement_api_name);
                       if (success) {
                           return mrb_true_value();
                       }
                       #endif
                       return mrb_false_value();
                   }
               }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "set_local_name", {
                                   [](mrb_state* state, mrb_value self) {
                                       char* name;
                                       mrb_get_args(state, "z", &name);
                                       Unet::ServiceEnet::SetLocalUsername(name);
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "get_local_name", {
                                   [](mrb_state* state, mrb_value self) {
                                       return mrb_str_new_cstr(state, get_local_user_name().c_str());
                                   }
                               }, MRB_ARGS_REQ(0));

    mrb_define_module_function(state, module, "get_build_info", {
                                   [](mrb_state* mrb, mrb_value self) {
                                       //auto enet_version = linked_version(mrb, self);
                                       auto result = mrb_hash_new(mrb);
                                       //cext_hash_set_kstr(mrb, result, "enet", enet_version);

                                       auto meta_host_platform = (const char*)META_HOST_PLATFORM;
                                       auto meta_platform = (const char*)META_PLATFORM;
                                       auto meta_type = (const char*)META_TYPE;
                                       auto meta_git_hash = (const char*)META_GIT_HASH;
                                       auto meta_git_branch = (const char*)META_GIT_BRANCH;
                                       //auto meta_git_repo = (const char *) META_GIT_REPO;
                                       auto meta_compiler_id = (const char*)META_COMPILER_ID;
                                       auto meta_compiler_version = (const char*)META_COMPILER_VERSION;

                                       auto build_information = mrb_hash_new(mrb);
                                       cext_hash_set_kstr(mrb, build_information, "host_platform",
                                                          mrb_str_new_cstr(mrb, meta_host_platform));
                                       cext_hash_set_kstr(mrb, build_information, "target_platform",
                                                          mrb_str_new_cstr(mrb, meta_platform));
                                       cext_hash_set_kstr(mrb, build_information, "build_type",
                                                          mrb_str_new_cstr(mrb, meta_type));
                                       cext_hash_set_kstr(mrb, build_information, "git_hash",
                                                          mrb_str_new_cstr(mrb, meta_git_hash));
                                       cext_hash_set_kstr(mrb, build_information, "git_branch",
                                                          mrb_str_new_cstr(mrb, meta_git_branch));
                                       cext_hash_set_kstr(mrb, build_information, "date",
                                                          mrb_str_new_cstr(mrb, __TIMESTAMP__));

                                       auto compiler = mrb_hash_new(mrb);
                                       cext_hash_set_kstr(mrb, compiler, "id",
                                                          mrb_str_new_cstr(mrb, meta_compiler_id));
                                       cext_hash_set_kstr(mrb, compiler, "version",
                                                          mrb_str_new_cstr(mrb, meta_compiler_version));

                                       cext_hash_set_kstr(mrb, build_information, "compiler", compiler);

                                       cext_hash_set_kstr(mrb, result, "build_information", build_information);

                                       return result;
                                   }
                               }, MRB_ARGS_REQ(0));

    mrb_define_module_function(state, module, "service_enabled?", {
                                   [](mrb_state* mrb, mrb_value self) {
                                       mrb_sym symbol;
                                       mrb_get_args(mrb, "n", &symbol);
                                       auto sym_hash = pext_symbol_komihash(mrb, symbol);

                                       if (sym_hash == pext_symbol_komihash(mrb, os_enet)) {
                                           if (use_enet) {
                                               return mrb_true_value();
                                           }
                                           return mrb_false_value();
                                       }
                                       if (sym_hash == pext_symbol_komihash(mrb, os_steam)) {
                                           if (use_steam) {
                                               return mrb_true_value();
                                           }
                                           return mrb_false_value();
                                       }
                                       if (sym_hash == pext_symbol_komihash(mrb, os_galaxy)) {
                                           return mrb_false_value();
                                       }

                                       ERR_INV_SERVICE
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "service_active?", {
                                   [](mrb_state* mrb, mrb_value self) {
                                       mrb_sym symbol;
                                       mrb_get_args(mrb, "n", &symbol);
                                       auto sym_hash = pext_symbol_komihash(mrb, symbol);

                                       if (sym_hash == pext_symbol_komihash(mrb, os_enet)) {
                                           if (use_enet) {
                                               return mrb_true_value();
                                           }
                                           return mrb_false_value();
                                       }
                                       if (sym_hash == pext_symbol_komihash(mrb, os_steam)) {
                                           if (use_steam) {
                                               return mrb_true_value();
                                           }
                                           return mrb_false_value();
                                       }
                                       if (sym_hash == pext_symbol_komihash(mrb, os_galaxy)) {
                                           return mrb_false_value();
                                       }

                                       ERR_INV_SERVICE
                                       return mrb_nil_value();
                                   }
                               }, MRB_ARGS_REQ(1));
}

mrb_value steam_init_api_m(mrb_state* state, mrb_value self) {
    update_state = state;

    auto str = get_argv(state);
    use_steam = !regexContains(str, "--nosteam");
    //use_enet = regexContains(str, "--enet");

    LOG_INFO("Loaded OService!\n");

    register_ruby_calls(state, steam);
    init_unet();

    LOG_INFO("args: " + str);

    auto to_join = regexSearch(str, STEAM_CALL_JOIN);
    to_join = regexReplace(to_join, "\\+connect_lobby\\ +", "");

    lobby_to_join = to_join;

    LOG_INFO("to_join: " + to_join);

    return mrb_nil_value();
}

void register_symbols(mrb_state* state) {
    REGISTER_SYMBOL(os_public)
    REGISTER_SYMBOL(os_private)
    REGISTER_SYMBOL(os_friends)

    REGISTER_SYMBOL(os_host)
    REGISTER_SYMBOL(os_client)

    REGISTER_SYMBOL(os_reliable)
    REGISTER_SYMBOL(os_unreliable)

    REGISTER_SYMBOL(on_data_received)
    REGISTER_SYMBOL(on_lobby_data_changed)

    REGISTER_SYMBOL(os_steam)
    REGISTER_SYMBOL(os_enet)
    REGISTER_SYMBOL(os_galaxy)
}

//DRB_FFI
extern "C" {
DRB_FFI_EXPORT
void drb_register_c_extensions_with_api(mrb_state* state, drb_api_t* api) {
    API = api;

    drb_gtk = mrb_module_get(state, "GTK");
    drb_runtime = mrb_class_get_under(state, drb_gtk, "Runtime");
    drb_console = mrb_class_get_under(state, drb_gtk, "Console");

    #if !defined(META_PLATFORM)     || \
!defined(META_TYPE)             || \
!defined(META_GIT_HASH)         || \
!defined(META_GIT_BRANCH)       || \
!defined(META_COMPILER_ID)      || \
!defined(META_COMPILER_VERSION)
    #error "Missing some Meta information. See CMakeLists.txt for more information.\n"
    #endif

    mrb_load_string(state, ruby_exceptions_code);
    mrb_load_string(state, ruby_socket_code);
    steam = mrb_module_get(state, "OService");

    exception_byte_buffer = mrb_class_get_under(state, steam, "ByteBufferError");
    exception_invalid_packet = mrb_class_get_under(state, steam, "InvalidPacketTypeError");
    exception_invalid_visibility = mrb_class_get_under(state, steam, "InvalidVisibilityError");
    exception_invalid_service = mrb_class_get_under(state, steam, "InvalidServiceTypeError");
    exception_serialization = mrb_class_get_under(state, steam, "InvalidPacketTypeError");
    exception_deserialization = mrb_class_get_under(state, steam, "InvalidVisibilityError");

    mrb_define_module_function(state, steam, "init_api", steam_init_api_m, MRB_ARGS_NONE());
    register_symbols(state);
    printf("* INFO: C extension 'OService' registration completed.\n");
}
}
