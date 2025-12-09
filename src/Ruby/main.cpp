#include <dragonruby.h>
#include <string>
#include <fstream>
#include <enet/enet.h>

#include "socket.rb.h"
#include "exceptions.rb.h"
#if defined(UNET_MODULE_STEAM)
#include <steam/steam_api.h>
#include <steam/steam_api_flat.h>
#endif
#include <filesystem>
#include <Unet/Services/ServiceEnet.h>

#include "Unet.h"
#include <bytebuffer/buffer.h>
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

mrb_state *update_state;

RClass *drb_gtk;
RClass *drb_runtime;
RClass *drb_console;
RClass *steam;

RClass *exception_byte_buffer;
RClass *exception_invalid_packet;
RClass *exception_invalid_visibility;
RClass *exception_invalid_service;
RClass *exception_serialization;
RClass *exception_deserialization;

static Unet::IContext *g_ctx = nullptr;
static Unet::LobbyListResult g_lastLobbyList;
static bool use_steam = true;
static bool use_enet = false;
static bool use_galaxy = false;
static bool g_steamEnabled = false;
static bool g_galaxyEnabled = false;
static bool g_enetEnabled = false;
std::string lobby_to_join;

std::string get_argv(mrb_state *state);

#include "Callbacks.h"

#if defined(UNET_MODULE_STEAM)
static void InitializeSteam() {
    bool open = false;
    std::ifstream app_file;
#if defined(PLATFORM_WINDOWS)
    SetEnvironmentVariableA("SteamAppId", STEAM_APP_ID);
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    setenv("SteamAppId", STEAM_APP_ID, 1);
#endif

    LOG_INFO("Enabling Steam service for " + appIdStr + ".");

    g_steamEnabled = SteamAPI_Init();
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
void printr_dbg(const std::string& str) {}
#endif

std::string get_argv(mrb_state *state) {
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

bool write_file(const std::string& file_path, ByteBuffer *buffer) {
    PHYSFS_ErrorCode error;
    const char* error_code;

    std::filesystem::path path(file_path);
    auto file_name = path.filename();
    auto dir_path = path.parent_path();

    auto mkdir_res = API->PHYSFS_mkdir(dir_path.generic_u8string().c_str());
    if (mkdir_res == 0) {
        error = API->PHYSFS_getLastErrorCode();
        error_code = API->PHYSFS_getErrorByCode(error);
        auto str_error = "write file error: " + std::string(error_code);
        printr(str_error);
        return false;
    }

    auto file = API->PHYSFS_openWrite(path.generic_u8string().c_str());
    if (file == nullptr) {
        error = API->PHYSFS_getLastErrorCode();
        error_code = API->PHYSFS_getErrorByCode(error);
        auto str_error = "write file error: " + std::string(error_code);
        printr(str_error);
        return false;
    }
    auto written_bytes = API->PHYSFS_writeBytes(file, buffer->Data(), buffer->Size());
    if (written_bytes != buffer->Size()) {
        error = API->PHYSFS_getLastErrorCode();
        error_code = API->PHYSFS_getErrorByCode(error);
        auto str_error = "write file error: " + std::string(error_code);
        printr(str_error);
        API->PHYSFS_close(file);
        return false;
    }
    API->PHYSFS_close(file);
    return true;
}

ByteBuffer* read_file(const std::string& path) {
    auto file = API->PHYSFS_openRead(path.c_str());
    if (file == nullptr) {
        return nullptr;
    }
    auto length = API->PHYSFS_fileLength(file);
    if (length == -1) {
        API->PHYSFS_close(file);
        return nullptr;
    }
    auto *buffer = new ByteBuffer(length);
    auto read_bytes = API->PHYSFS_readBytes(file, buffer->MutableData(), buffer->Size());
    if (read_bytes != length) {
        delete buffer;
        API->PHYSFS_close(file);
        return nullptr;
    }
    API->PHYSFS_close(file);
    return buffer;
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
        enet_initialize();
        LOG_INFO("Enabled module: Enet");
        g_ctx->EnableService(Unet::ServiceType::Enet);
        g_enetEnabled = true;
    }
}

void register_ruby_calls(mrb_state *state, drb_api_t *api, RClass *module) {
    mrb_define_module_function(state, module, "__update_service", {
                               [](mrb_state *state, mrb_value self) {
                                   update_state = state;
#if defined(UNET_MODULE_STEAM)
                                   if (g_steamEnabled) {
                                       SteamAPI_RunCallbacks();
                                   }
#endif
                                   g_ctx->RunCallbacks();
                                   //return array;
                                   const int max_channels = 1;
                                   for (int i = 0; i < max_channels; ++i) {
                                       auto data = g_ctx->ReadMessage(i);
                                       while (data != nullptr) {

                                           auto buffer = new ByteBuffer(
                                               data.get()->m_data, data.get()->m_size, false); //TODO: free!
                                           buffer->Uncompress();

                                           auto result = start_deserialize_data(buffer, state);

                                           delete buffer;

                                           auto mrb_data = mrb_hash_new_capa(update_state, 3);
                                           pext_hash_set(update_state, mrb_data, "data", result);
                                           auto peer = mrb_hash_new_capa(update_state, 2);
                                           pext_hash_set(update_state, peer, "id",
                                                         std::to_string(data.get()->m_peer.ID));
                                           pext_hash_set(update_state, peer, "service",
                                                         GetServiceNameByType(data.get()->m_peer.Service));
                                           pext_hash_set(update_state, mrb_data, "peer", peer);
                                           pext_hash_set(update_state, mrb_data, "channel", data.get()->m_channel);
                                           push_to_updates(on_data_received, mrb_data);
                                           data = g_ctx->ReadMessage(i);
                                       }
                                   }

                                   auto m_array = mrb_ary_new_capa(state, value_list.size());
                                   for (int i = 0; i < value_list.size(); i++) {
                                       mrb_ary_set(state, m_array, i, value_list[i]);
                                   }
                                   value_list.clear();
                                    mrb_funcall(
                                        state, self, "__exec_callback", 1, m_array);
                                       return mrb_nil_value();
                                   }
                           }, MRB_ARGS_NONE());

    mrb_define_module_function(state, module, "direct_connect", {
                           [](mrb_state *state, mrb_value self) {
                               printr_dbg("Connecting to Enet!\n");
                               mrb_value ip_str;
                               mrb_int port;
                               mrb_get_args(state, "Si", &ip_str, &port);

                               ENetAddress addr;
                               enet_address_set_host(&addr, cext_to_string(state,ip_str));
                               addr.port = port;
                               g_ctx->JoinLobby(Unet::ServiceID(Unet::ServiceType::Enet, *(uint64_t*)&addr));

                               return mrb_nil_value();
                           }
                       }, MRB_ARGS_REQ(2));

    mrb_define_module_function(state, module, "create_lobby", {
                               [](mrb_state *state, mrb_value self) {
                                   printr_dbg("LobbyCreating!\n");
                                   mrb_value chat_str;
                                   mrb_int lobby_size;
                                   mrb_sym type;
                                   mrb_get_args(state, "Sin", &chat_str, &lobby_size, &type);
                                   auto lobby_type = Unet::LobbyPrivacy::Private;
                                   if (type == os_private) {
                                       lobby_type = Unet::LobbyPrivacy::Private;
                                   }
                                   else if (type == os_friends) {
                                        lobby_type = Unet::LobbyPrivacy::FriendsOnly;
                                   }
                                   else if (type == os_public) {
                                       lobby_type = Unet::LobbyPrivacy::Public;
                                   }
                                   else {
                                       ERR_INV_VISIBILITY
                                       return mrb_nil_value();
                                   }
                                   g_ctx->CreateLobby(lobby_type, (int)lobby_size,
                                                      mrb_str_to_cstr(state, chat_str));
                                   return mrb_nil_value();
                               }
                           }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "get_lobby_to_join", {
                               [](mrb_state *state, mrb_value self) {
                                   if (lobby_to_join.empty()) {
                                       return mrb_nil_value();
                                   }
                                   return mrb_str_new_cstr(state, lobby_to_join.c_str());
                               }
                           }, MRB_ARGS_REQ(0));

    mrb_define_module_function(state, module, "get_lobby_to_join!", {
                           [](mrb_state *state, mrb_value self) {
                               if (lobby_to_join.empty()) {
                                   return mrb_nil_value();
                               }
                               auto return_value = mrb_str_new_cstr(state, lobby_to_join.c_str());
                               lobby_to_join.clear();
                               return return_value;
                           }
                       }, MRB_ARGS_REQ(0));

    mrb_define_module_function(state, module, "list_lobbies", {
                               [](mrb_state *state, mrb_value self) {
                                   printr_dbg("Listing Lobbies!\n");
                                   Unet::LobbyListFilter filter;
                                   g_ctx->GetLobbyList(filter);
                                   return mrb_nil_value();
                               }
                           }, MRB_ARGS_NONE());

    mrb_define_module_function(state, module, "join_lobby", {
                               [](mrb_state *state, mrb_value self) {
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
                               [](mrb_state *state, mrb_value self) {
                                   printr_dbg("Joining Lobby by id!\n");
                                   mrb_value lobby_num;
                                   mrb_get_args(state, "S", &lobby_num);
                                   std::string lobby_id = mrb_str_to_cstr(state, lobby_num);
                                   Unet::LobbyInfo lobby_info;
                                   bool found = false;
                                   for (auto & lobby : g_lastLobbyList.Lobbies) {
                                       if (found) {
                                           break;
                                       }
                                       auto lobby_entry_points = lobby.EntryPoints;
                                       for (auto & lobby_entry_point : lobby_entry_points) {
                                           auto entry_point = lobby_entry_point.ID;
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
                               [](mrb_state *state, mrb_value self) {
                                   printr_dbg("Leaving Lobby!\n");
                                   g_ctx->LeaveLobby();
                                   return mrb_nil_value();
                               }
                           }, MRB_ARGS_NONE());

    mrb_define_module_function(state, module, "set_lobby_name", {
                               [](mrb_state *state, mrb_value self) {
                                   printr_dbg("Setting lobby name!\n");
                                   mrb_value lobby_str;
                                   mrb_get_args(state, "S", &lobby_str);
                                   auto current_lobby = g_ctx->CurrentLobby();
                                   if (current_lobby == nullptr) {
                                       LOG_ERROR("Not in a lobby");
                                       return mrb_nil_value();
                                   }

                                   current_lobby->SetName(mrb_str_to_cstr(state, lobby_str));
                                   return mrb_nil_value();
                               }
                           }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "show_lobby_data", {
                               [](mrb_state *state, mrb_value self) {
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
                           [](mrb_state *state, mrb_value self) {
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
                               [](mrb_state *state, mrb_value self) {
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
                               [](mrb_state *state, mrb_value self) {
                                   mrb_value data;
                                   mrb_int channel = 0;
                                   mrb_sym rel_type = os_reliable;
                                   mrb_get_args(state, "H|in", &data, &channel, &rel_type);
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

                                   auto buffer = new ByteBuffer();
                                   start_serialize_data(buffer, state, data);
                                   if (!buffer->Compress()) {
                                       //LOG_ERROR("Compression failed!");
                                       ERR_BIN_BUFF
                                       delete buffer;
                                       return mrb_nil_value();
                                   }

                                   auto type = Unet::PacketType::Reliable;
                                   if (rel_type == os_reliable) {
                                       type = Unet::PacketType::Reliable;
                                   } else if (rel_type == os_unreliable) {
                                       type = Unet::PacketType::Unreliable;
                                   } else {
                                       ERR_INV_PACKET
                                       delete buffer;
                                       return mrb_nil_value();
                                   }

                                   //TODO: fix unreal
                                   //type = Unet::PacketType::Reliable;

                                   g_ctx->SendToHost((uint8_t *) buffer->Data(), buffer->Size(), type, channel); //TODO: fix unreal
                                   delete buffer;
                                   return mrb_nil_value();
                               }
                           }, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(2));

    mrb_define_module_function(state, module, "send_to", {
                               [](mrb_state *state, mrb_value self) {
                                   if (!g_ctx->IsHosting()) {
                                       push_error("'send_to' is only available for host!", -1);
                                       return mrb_nil_value();
                                   }
                                   mrb_value data;
                                   mrb_int peer;
                                   mrb_int channel = 0;
                                   mrb_sym rel_type = os_reliable;
                                   mrb_get_args(state, "Hi|in", &data, &peer, &channel, &rel_type);
                                   auto current_lobby = g_ctx->CurrentLobby();
                                   if (current_lobby == nullptr) {
                                       LOG_ERROR("Not in a lobby.");
                                       return mrb_nil_value();
                                   }

                                   auto member = g_ctx->CurrentLobby()->GetMember(peer);

                                   auto buffer = new ByteBuffer();
                                   start_serialize_data(buffer, state, data);
                                   if (!buffer->Compress()) {
                                       LOG_ERROR("Compression failed!");
                                       delete buffer;
                                       return mrb_nil_value();
                                   }

                                   auto type = Unet::PacketType::Reliable;
                                     if (rel_type == os_reliable) {
                                         type = Unet::PacketType::Reliable;
                                     } else if (rel_type == os_unreliable) {
                                         type = Unet::PacketType::Unreliable;
                                     } else {
                                         ERR_INV_PACKET
                                         delete buffer;
                                         return mrb_nil_value();
                                     }

                                     //TODO: fix unreal
                                     //type = Unet::PacketType::Reliable;

                                   g_ctx->SendTo(member, (uint8_t *) buffer->Data(), buffer->Size(), type, channel);
                                   delete buffer;
                                   return mrb_nil_value();
                               }
                           }, MRB_ARGS_REQ(2) | MRB_ARGS_OPT(2));

    mrb_define_module_function(state, module, "send_to_members", {
                               [](mrb_state *state, mrb_value self) {
                                   if (!g_ctx->IsHosting()) {
                                       push_error("'send_to_members' is only available for host!", -1);
                                       return mrb_nil_value();
                                   }
                                   mrb_value data;
                                   mrb_int channel = 0;
                                   mrb_sym rel_type = os_reliable;
                                   mrb_get_args(state, "H|in", &data, &channel, &rel_type);
                                   auto current_lobby = g_ctx->CurrentLobby();
                                   if (current_lobby == nullptr) {
                                       LOG_ERROR("Not in a lobby.");
                                       return mrb_nil_value();
                                   }

                                   auto buffer = new ByteBuffer();
                                   start_serialize_data(buffer, state, data);
                                   if (!buffer->Compress()) {
                                       LOG_ERROR("Compression failed!");
                                       delete buffer;
                                       return mrb_nil_value();
                                   }

                                   auto type = Unet::PacketType::Reliable;
                                    if (rel_type == os_reliable) {
                                        type = Unet::PacketType::Reliable;
                                    } else if (rel_type == os_unreliable) {
                                        type = Unet::PacketType::Unreliable;
                                    } else {
                                        ERR_INV_PACKET
                                        delete buffer;
                                        return mrb_nil_value();
                                    }

                                    //TODO: fix unreal
                                    //type = Unet::PacketType::Reliable;

                                   g_ctx->SendToAll((uint8_t *) buffer->Data(), buffer->Size(), type, channel); //TODO: fix unreal
                                   delete buffer;
                                   return mrb_nil_value();
                               }
                           }, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(2));

    mrb_define_module_function(state, module, "debug_in", {
                               [](mrb_state *state, mrb_value self) {
                                   mrb_value data;
                                   mrb_get_args(state, "S", &data);

                                   auto bin_str = mrb_str_to_cstr(state, data);

                                   auto input_buffer = read_file(bin_str);
                                   if (input_buffer == nullptr) {
                                       return mrb_nil_value();
                                   }

                                   auto deserialized_data = start_deserialize_data(input_buffer, state);

                                   auto hash = komihash(input_buffer->Data(), input_buffer->Size(), 0);
                                   printr(std::to_string(hash));

                                   delete input_buffer;
                                   return deserialized_data;
                               }
                           }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "debug_out", {
                               [](mrb_state *state, mrb_value self) {
                                   mrb_value data;
                                   mrb_get_args(state, "H", &data);

                                   auto buffer = new ByteBuffer();
                                   start_serialize_data(buffer, state, data);

                                   auto random_str = std::string("/debug/").append(epoch_string_ms().append(".ossp"));

                                   auto result = write_file(random_str, buffer);

                                   delete buffer;
                                   return pext_str(state, random_str);
                               }
                           }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "read_file", {
                               [](mrb_state *state, mrb_value self) {
                                   mrb_value data;
                                   mrb_get_args(state, "S", &data);

                                   auto bin_str = mrb_str_to_cstr(state, data);

                                   auto input_buffer = read_file(bin_str);
                                   if (input_buffer == nullptr) {
                                       return mrb_nil_value();
                                   }

                                   //input_buffer->Uncompress();
                                   auto deserialized_data = start_deserialize_data(input_buffer, state);

                                   auto hash = komihash(input_buffer->Data(), input_buffer->Size(), 0); //TODO: return

                                   delete input_buffer;
                                   return deserialized_data;
                               }
                           }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "write_file", {
                               [](mrb_state *state, mrb_value self) {
                                   mrb_value path;
                                   mrb_value data;
                                   mrb_get_args(state, "SH", &path, &data);

                                   auto str_path = mrb_str_to_cstr(state, path);

                                   auto buffer = new ByteBuffer();
                                   start_serialize_data(buffer, state, data);
                                   //buffer->Compress();

                                   auto hash = komihash(buffer->Data(), buffer->Size(), 0);

                                   auto result = write_file(str_path, buffer);
                                   delete buffer;

                                   if (!result) {
                                       return mrb_false_value();
                                   }

                                   return pext_str(state, std::to_string(hash));
                               }
                           }, MRB_ARGS_REQ(2));

    mrb_define_module_function(state, module, "send_chat", {
                               [](mrb_state *state, mrb_value self) {
                                   mrb_value chat_str;
                                   mrb_get_args(state, "S", &chat_str);
                                   auto str = mrb_str_to_cstr(state, chat_str);
                                   g_ctx->SendChat(str);
                                   return mrb_nil_value();
                               }
                           }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "set_lobby_data", {
                               [](mrb_state *state, mrb_value self) {
                                   mrb_value key_str;
                                   mrb_value value_str;
                                   mrb_get_args(state, "SS", &key_str, &value_str);
                                   auto k_str = mrb_str_to_cstr(state, key_str);
                                   auto v_str = mrb_str_to_cstr(state, value_str);

                                   auto current_lobby = g_ctx->CurrentLobby();
                                   if (current_lobby == nullptr) {
                                       LOG_ERROR("Not in a lobby.");
                                       return mrb_nil_value();
                                   }

                                   if (!current_lobby->GetInfo().IsHosting) {
                                       LOG_ERROR("Lobby data can only be set by the host.");
                                       return mrb_nil_value();
                                   }

                                   current_lobby->SetData(k_str, v_str);
                                   return mrb_nil_value();
                               }
                           }, MRB_ARGS_REQ(2));

    mrb_define_module_function(state, module, "remove_lobby_data", {
                               [](mrb_state *state, mrb_value self) {
                                   mrb_value rem_str;
                                   mrb_get_args(state, "S", &rem_str);
                                   auto current_lobby = g_ctx->CurrentLobby();
                                   if (current_lobby == nullptr) {
                                       LOG_ERROR("Not in a lobby.");
                                       return mrb_nil_value();;
                                   }

                                   if (!current_lobby->GetInfo().IsHosting) {
                                       LOG_ERROR("Lobby data can only be removed by the host.");
                                       return mrb_nil_value();;
                                   }

                                   current_lobby->RemoveData(mrb_str_to_cstr(state, rem_str));
                                   return mrb_nil_value();
                               }
                           }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "kick_member", {
                               [](mrb_state *state, mrb_value self) {
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
                               [](mrb_state *state, mrb_value self) {
                                   return mrb_bool_value(g_ctx->IsHosting());
                               }
                           }, MRB_ARGS_REQ(0));

    mrb_define_module_function(state, module, "connected?", {
                           [](mrb_state *state, mrb_value self) {
                               auto current_lobby = g_ctx->CurrentLobby();
                               if (current_lobby == nullptr) {
                                   return mrb_bool_value(false);
                               }
                               return mrb_bool_value(true);
                           }
                       }, MRB_ARGS_REQ(0));

    mrb_define_module_function(state, module, "get_ping", {
                       [](mrb_state *state, mrb_value self) {
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
                           [](mrb_state *state, mrb_value self) {
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
                           [](mrb_state *state, mrb_value self) {
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
                           [](mrb_state *state, mrb_value self) {
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
                           [](mrb_state *state, mrb_value self) {
                               mrb_value token_0;
                               mrb_value token_1;
                                   mrb_get_args(state, "SS", &token_0, &token_1);
#if defined(UNET_MODULE_STEAM)
                               auto friends = SteamFriends();
                               if (friends == nullptr) {
                                   return mrb_nil_value();
                               }

                               auto str_0 = std::string(mrb_string_cstr(state, token_0));
                               auto str_1 = std::string(mrb_string_cstr(state, token_1));
                               str_0.resize(k_cchMaxRichPresenceKeyLength);
                               str_1.resize(k_cchMaxRichPresenceValueLength);

                               SteamAPI_ISteamFriends_SetRichPresence(friends, str_0.c_str(), str_1.c_str());
#endif
                               return mrb_nil_value();
                           }
                       }, MRB_ARGS_REQ(2));

    mrb_define_module_function(state, module, "set_local_name", {
                           [](mrb_state *state, mrb_value self) {
                               mrb_value name;
                                mrb_get_args(state, "S", &name);
                               auto name_to_set = mrb_str_to_cstr(state, name);
                               Unet::ServiceEnet::SetLocalUsername(name_to_set);
                               return mrb_nil_value();
                           }
                       }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "get_local_name", {
                       [](mrb_state *state, mrb_value self) {
                           return mrb_str_new_cstr(state, get_local_user_name().c_str());
                       }
                   }, MRB_ARGS_REQ(0));

    mrb_define_module_function(state, module, "get_build_info", {
                                        [](mrb_state *mrb, mrb_value self) {
                                            //auto enet_version = linked_version(mrb, self);
                                            auto result = mrb_hash_new(mrb);
                                            //cext_hash_set_kstr(mrb, result, "enet", enet_version);

                                            auto meta_host_platform = (const char *) META_HOST_PLATFORM;
                                            auto meta_platform = (const char *) META_PLATFORM;
                                            auto meta_type = (const char *) META_TYPE;
                                            auto meta_git_hash = (const char *) META_GIT_HASH;
                                            auto meta_git_branch = (const char *) META_GIT_BRANCH;
                                            //auto meta_git_repo = (const char *) META_GIT_REPO;
                                            auto meta_compiler_id = (const char *) META_COMPILER_ID;
                                            auto meta_compiler_version = (const char *) META_COMPILER_VERSION;

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
                          [](mrb_state *state, mrb_value self) {
                              mrb_sym symbol;
                              mrb_get_args(state, "n", &symbol);
                              auto sym_hash = pext_symbol_komihash(state, symbol);

                              if (sym_hash == pext_symbol_komihash(state, os_enet)) {
                                  if (use_enet) {
                                      return mrb_true_value();
                                  }
                                  return mrb_false_value();
                              }
                              if (sym_hash == pext_symbol_komihash(state, os_steam)) {
                                  if (use_steam) {
                                      return mrb_true_value();
                                  }
                                  return mrb_false_value();
                              }
                              if (sym_hash == pext_symbol_komihash(state, os_galaxy)) {
                                  return mrb_false_value();
                              }

                              ERR_INV_SERVICE
                              return mrb_nil_value();
                          }
                      }, MRB_ARGS_REQ(1));

    mrb_define_module_function(state, module, "service_active?", {
                      [](mrb_state *state, mrb_value self) {
                          mrb_sym symbol;
                          mrb_get_args(state, "n", &symbol);
                          auto sym_hash = pext_symbol_komihash(state, symbol);

                          if (sym_hash == pext_symbol_komihash(state, os_enet)) {
                              if (use_enet) {
                                  return mrb_true_value();
                              }
                              return mrb_false_value();
                          }
                          if (sym_hash == pext_symbol_komihash(state, os_steam)) {
                              if (use_steam) {
                                  return mrb_true_value();
                              }
                              return mrb_false_value();
                          }
                          if (sym_hash == pext_symbol_komihash(state, os_galaxy)) {
                              return mrb_false_value();
                          }

                          ERR_INV_SERVICE
                          return mrb_nil_value();
                          }
                        }, MRB_ARGS_REQ(1));
}

mrb_value steam_init_api_m(mrb_state *state, mrb_value self) {
    update_state = state;

    auto str = get_argv(state);
    use_steam = !regexContains(str, "--nosteam");
    use_enet = regexContains(str, "--enet");

    LOG_INFO("Loaded OService!\n");

    register_ruby_calls(state, drb_api, steam);
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
void drb_register_c_extensions_with_api(mrb_state *state, drb_api_t *api) {
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
