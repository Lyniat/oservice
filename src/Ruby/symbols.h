#pragma once
#define REGISTER_SYMBOL(SYM) SYM = mrb_intern_cstr(state, #SYM);

inline mrb_sym os_private;
inline mrb_sym os_public;
inline mrb_sym os_friends;

inline mrb_sym os_host;
inline mrb_sym os_client;

inline mrb_sym os_reliable;
inline mrb_sym os_unreliable;

inline mrb_sym on_data_received;
inline mrb_sym on_lobby_data_changed;
inline mrb_sym on_lobby_self_joined;
inline mrb_sym on_lobby_self_left;
inline mrb_sym on_lobby_player_joined;
inline mrb_sym on_lobby_player_left;
inline mrb_sym on_lobby_list;
inline mrb_sym on_lobby_chat;
inline mrb_sym on_lobby_name_changed;
inline mrb_sym on_lobby_max_players_changed;
inline mrb_sym on_lobby_info_fetched;
inline mrb_sym on_lobby_created;
inline mrb_sym on_lobby_member_data_changed;
inline mrb_sym on_lobby_member_name_changed;
inline mrb_sym on_lobby_file_added;
inline mrb_sym on_lobby_file_removed;
inline mrb_sym on_lobby_file_requested;
inline mrb_sym on_lobby_file_data_send_progress;
inline mrb_sym on_lobby_file_data_send_finished;
inline mrb_sym on_lobby_file_data_receive_progress;
inline mrb_sym on_lobby_file_data_receive_finished;

inline mrb_sym os_steam;
inline mrb_sym os_enet;
inline mrb_sym os_galaxy;