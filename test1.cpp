#include "log.hpp"
#include <iostream>
#include <pthread.h>
#include <unistd.h>
using namespace std;

bool m_close_log = false;

void* callback(void* args) {
    int* num = (int*)args;

    for (int i = 0; i < 110; ++i) {
        switch(*num) {
            case 0:
                LOG_DEBUG("this is a bug!");
                break;
            case 1:
                LOG_ERROR("this is an error!");
                break;
            case 2:
                LOG_INFO("this is a piece of information");
                break;
            case 3:
                LOG_WARN("this is a warning");
                break;
            case 4:
                LOG_FATAL("this is a fatal error");
                break;
            default:
                break;
        }
    }

    delete num;
    return nullptr;
}

int main() {
    Log* log = Log::get_instance();
    log->init("async_write", m_close_log, 8192, 500, 50);
    pthread_t threads[5];
    for (int i = 0; i < 5; ++i) {
        int *num = new int(i);
        pthread_create(&threads[i], NULL, callback, num);
        pthread_detach(threads[i]);
    }

    sleep(10);
}

