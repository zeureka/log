#include "log.hpp"
#include <iostream>
#include <unistd.h>
using namespace std;

int main() {
    for (int i = 0; i < 550; ++i) {
        LOG_INFO("Single Thread Insert Data%s", "D");
        usleep(5000);
    }

    Log::get_instance()->set_dir_name("LogFile/log");

    for (int i = 0; i < 550; ++i) {
        LOG_INFO("Single Thread Insert Data%s", "D");
        usleep(5000);
    }
}

