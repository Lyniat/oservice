#pragma once

#include <dragonruby.h>
#include <ossp/help.h>
#include <string>
#include <fmt/printf.h>
#include <ossp/api.h>

typedef enum console_output_t {
    PRINT_LOG,
    PRINT_WARNING,
    PRINT_ERROR
} console_output_t;

//#define LOG_INFO printr
#define LOG_INFO(...) printr(fmt::sprintf( __VA_ARGS__ ))
#define LOG_ERROR(...) printr(fmt::sprintf( __VA_ARGS__ ))
#define LOG_DEBUG(...) printr(fmt::sprintf( __VA_ARGS__ ))
#define LOG_WARN(...) printr(fmt::sprintf( __VA_ARGS__ ))

#define LOG_FROM_CALLBACK(...) printr(fmt::sprintf( __VA_ARGS__ ))

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
