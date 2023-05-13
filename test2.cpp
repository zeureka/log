#include "log.hpp"
#include <iostream>
#include <unistd.h>
using namespace std;

int main() {
    bool m_close_log = false;
    Log* log = Log::get_instance();
    log->init("single_write", m_close_log, 8192, 500);
    for (int i = 0; i < 550; ++i) {
        LOG_INFO("Single Thread Insert Data%s", "D");
        usleep(5000);
    }
}

