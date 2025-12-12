#include "print.h"

#include "api.h"

void printr(std::string str) {
    print(update_state, PRINT_LOG, str.c_str());
}