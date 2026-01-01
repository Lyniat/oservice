#include "api.h"
#include <ossp/api.h>
#include <ossp/help.h>

enet_uint16 enet_default_port = ENET_DEFAULT_PORT;

std::vector<mrb_value> value_list;
std::vector<mrb_value> own_data_list;

#define __temp_state update_state

void push_to_updates(const std::string& event_type, mrb_value value) {
    auto hash = mrb_hash_new_capa(update_state, 2);
    PEXT_H(hash, "type", event_type);
    PEXT_H(hash, "data", value);
    value_list.push_back(hash);
}

void push_to_updates(mrb_sym event_type, mrb_value value) {
    auto hash = mrb_hash_new_capa(update_state, 2);
    PEXT_H(hash, "type", event_type);
    PEXT_H(hash, "data", value);
    value_list.push_back(hash);
}

void push_error(const std::string& event_type, int id) {
    auto hash = mrb_hash_new_capa(update_state, 3);
    PEXT_H(hash, "type", "error");
    PEXT_H(hash, "data", event_type);
    PEXT_H(hash, "id", id);
    value_list.push_back(hash);
}
#undef __temp_state