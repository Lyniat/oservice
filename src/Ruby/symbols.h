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

inline mrb_sym os_steam;
inline mrb_sym os_enet;
inline mrb_sym os_galaxy;