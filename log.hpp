#ifndef LOG_HPP
#define LOG_HPP

#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <atomic>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <condition_variable>


/******************************** definition ********************************/

enum class LogType : char { DEBUG, INFO, WARN, ERROR, FATAL };

class Log {
public:
    // 单例懒汉模式创建对象
    static Log* get_instance();

    // 异步模式线程回调函数
    static void* flush_log_thread(void* args);

    // 可选择的参数有日志文件名，日志缓冲区大小，最大日志行数，日志队列容量
    bool init(const char* file_name, bool close_log, int log_buf_size = 8192, 
              int split_lines = 5000000, int queue_capacity = 0);

    // 写入日志文件
    void write_log(LogType type, const char* format, ...);

    // 刷新缓冲区
    void flush(void);

private:
    Log();
    virtual ~Log();
    void* async_write_log();

private:
    char dir_name[128];         // 日志文件路径
    char log_name[128];         // log 文件名
    int m_split_lines;          // 日志最大行数
    int m_log_buf_size;         // 日志缓冲区大小
    long long m_count;          // 日志行数记录
    int m_today;                // 记录当天是哪一天
    FILE* m_fp;                 // 打开 log 的文件指针
    char* m_buf;                // 日志缓冲区指针
    bool m_is_async;            // 是否同步标志位
    bool m_close_log;           // 关闭日志
    int m_queue_capacity;       // 队列容量
    std::mutex m_mutex;                     // 互斥锁
    std::queue<std::string> m_log_queue;   // 阻塞队列
};

#define LOG_DEBUG(format, ...) \
    if (!m_close_log) { Log::get_instance()->write_log(LogType::DEBUG, format, ##__VA_ARGS__); Log::get_instance()->flush(); }
#define LOG_INFO(format, ...) \
    if (!m_close_log) { Log::get_instance()->write_log(LogType::INFO, format, ##__VA_ARGS__); Log::get_instance()->flush(); }
#define LOG_WARN(format, ...) \
    if (!m_close_log) { Log::get_instance()->write_log(LogType::WARN, format, ##__VA_ARGS__); Log::get_instance()->flush(); }
#define LOG_ERROR(format, ...) \
    if (!m_close_log) { Log::get_instance()->write_log(LogType::ERROR, format, ##__VA_ARGS__); Log::get_instance()->flush(); }
#define LOG_FATAL(format, ...) \
    if (!m_close_log) { Log::get_instance()->write_log(LogType::FATAL, format, ##__VA_ARGS__); Log::get_instance()->flush(); }


/******************************** implementation ********************************/

Log::Log() {
    m_count = 1;
    m_is_async = false;
}

Log::~Log() {
    // 防止关闭日志系统时，任务队列不为空
    Log::get_instance()->async_write_log();

    if (NULL != m_fp) {
        fclose(m_fp);
    }

    if (NULL != m_buf) {
        delete[] m_buf;
    }
}

Log* Log::get_instance() {
    // 使用静态局部变量创建一个单例对象，能实现线程安全
    static Log instance;
    return &instance;
}

void* Log::flush_log_thread(void* args) {
    Log::get_instance()->async_write_log();
    return NULL;
}

bool Log::init(const char* file_name, bool close_log, int log_buf_size, int split_lines, int queue_capacity) {
    m_close_log = close_log;
    m_split_lines = split_lines;
    m_log_buf_size = log_buf_size;
    m_queue_capacity = queue_capacity;
    m_buf = new char[m_log_buf_size];

    memset(m_buf, '\0', m_log_buf_size);
    memset(dir_name, '\0', sizeof(dir_name));
    memset(log_name, '\0', sizeof(log_name));

    time_t t = time(NULL);
    struct tm* sys_tm = localtime(&t); // 将 time_t 表示的时间转换为没有经过时区转换的UTC时间
    struct tm my_tm = *sys_tm;
    m_today = my_tm.tm_mday;

    // 在 file_name 所指向的字符串中搜索最后一次出现 '/' 的位置
    const char* p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    if (NULL == p) {
        strcpy(log_name, file_name);
        // 表示该日志文件在本级目录里面
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", 
                 my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    } else {
        // 表示该日志文件不在本级目录里面，需要把日志文件名和日志文件目录给记录下来
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", 
                dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    // fopen(打开的文件名，文件的访问模式)，"a":在尾部追加， 文件名不存在会自动创建
    m_fp = fopen(log_full_name, "a");

    if (NULL == m_fp) {
        return false;
    }

    // 如果设置了 max_queue_size，则设置为异步
    if (queue_capacity >= 1) {
        m_is_async = true;
        pthread_t tid;
        pthread_create(&tid, NULL, flush_log_thread, NULL);
        pthread_detach(tid);
    }

    return true;
}

void Log::write_log(LogType type, const char* format, ...) {
    struct timeval now{0, 0};
    gettimeofday(&now, NULL); // 获取当前系统时间
    time_t t = now.tv_sec;
    struct tm* sys_em = localtime(&t);
    struct tm my_tm = *sys_em;

    char typeStr[16] = {0};
    switch (type) {
    case LogType::DEBUG:
        strcpy(typeStr, "[DEBUG]: ");
        break;
    case LogType::INFO:
        strcpy(typeStr, "[INFO] : ");
        break;
    case LogType::WARN:
        strcpy(typeStr, "[WARN] : ");
        break;
    case LogType::ERROR:
        strcpy(typeStr, "[ERROR]: ");
        break;
    case LogType::FATAL:
        strcpy(typeStr, "[FATAL]: ");
        break;
    default:
        break;
    }

    // 写入一个 log
    m_mutex.lock();

    // 时间发生变化，或者 log 日志行数为最大日志行数的倍数
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) {
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp); // 关闭流 m_fp，释放文件指针和有关的缓冲区;
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if (m_today != my_tm.tm_mday) {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        } else {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }

        m_fp = fopen(new_log, "a");
    }

    m_mutex.unlock();

    // 类型 存储可变参数的信息
    va_list valst;
    // 宏定义 开始使用可变参数列表
    va_start(valst, format);

    m_mutex.lock();
    // 写入具体时间
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s", 
            my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, 
            my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_sec, typeStr);
    // 将一个可变参数（alst）格式化（format）输出到一个限定最大长度（m_log_buf_size - n - 1）的字符串缓冲区（m_buf）中
    int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valst);

    m_buf[m + n] = '\n';
    m_buf[m + n + 1] = '\0';

    m_mutex.unlock();

    m_mutex.lock();
    if (m_is_async && m_log_queue.size() < m_queue_capacity) {
        m_log_queue.push(std::string(m_buf));
    } else {
        fputs(m_buf, m_fp);
        m_count++;
    }
    m_mutex.unlock();

    // 宏定义 结束使用可变参数列表
    va_end(valst);
}

void Log::flush(void) {
    m_mutex.lock();
    // 刷新缓冲区，将缓冲区的数据写入 m_fp 所指的文件中
    fflush(m_fp);
    m_mutex.unlock();
}

void* Log::async_write_log() {
    std::string single_log{""};

    // 从阻塞队列中取出一个日志，写入文件
    m_mutex.lock();
    while (!m_log_queue.empty()) {
        single_log = m_log_queue.front();
        m_log_queue.pop();
        fputs(single_log.c_str(), m_fp);
        m_count++;
    }
    m_mutex.unlock();

    return NULL;
}

#endif
