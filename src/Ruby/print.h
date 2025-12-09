#pragma once

#include <dragonruby.h>
#include <ossp/help.h>
#include <string>
#include <ossp/api.h>

typedef enum console_output_t {
    PRINT_LOG,
    PRINT_WARNING,
    PRINT_ERROR
} console_output_t;

//#define LOG_INFO printr
#define LOG_INFO(s) printr(s)
#define LOG_ERROR(s) printr(s)
#define LOG_DEBUG(s) printr(s)
#define LOG_WARN(s) printr(s)

#define LOG_FROM_CALLBACK(s) printr(s)

template<typename... T>
mrb_value print(mrb_state *state, console_output_t type, const char *text, T &&... args) {
    if (type == PRINT_ERROR) {
        mrb_funcall(state, mrb_nil_value(), "raise", 1, mrb_str_new_cstr(state, text));
        return mrb_nil_value();
    }
    mrb_funcall(state, mrb_nil_value(), "puts", 1, mrb_str_new_cstr(state, text));
    return mrb_nil_value();
}

template<typename... T>
mrb_value print(mrb_state *state, const char *text, T &&... args) {
    return print(state, PRINT_LOG, text, args...);
}

void printr(std::string str);
