#pragma once
#include <string>
#include <vector>

#include <ossp/api.h>
#include <ossp/help.h>
#include <dragonruby.h>
extern drb_api_t* drb_api;

#ifndef API
#define API drb_api
#endif

extern std::vector<mrb_value> value_list;
extern std::vector<mrb_value> own_data_list;
extern mrb_state* update_state;

void push_to_updates(const std::string& event_type, mrb_value value);

void push_to_updates(mrb_sym event_type, mrb_value value);

void push_error(const std::string& event_type, int id);