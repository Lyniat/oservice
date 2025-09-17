#include "print.h"
#include <ossp/help.h>

void printr(std::string str) {
    print(update_state, PRINT_LOG, str.c_str());
}