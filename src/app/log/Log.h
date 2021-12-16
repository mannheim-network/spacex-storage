#ifndef _SPACEX_LOG_H_
#define _SPACEX_LOG_H_

#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <string>
#include <mutex>

#include "Resource.h"

#define SPACEX_LOG_BUF_SIZE  33554432 /* 32*1024*1024 */
#define SPACEX_LOG_INFO_TAG "INFO"
#define SPACEX_LOG_WARN_TAG "WARN"
#define SPACEX_LOG_ERR_TAG "ERROR"
#define SPACEX_LOG_DEBUG_TAG "DEBUG"

namespace spacex
{

class Log
{
public:
    static Log *log;
    static Log *get_instance();
    void set_debug(bool flag);
    void info(const char *format, ...);
    void warn(const char *format, ...);
    void err(const char *format, ...);
    void debug(const char *format, ...);
    bool get_debug_flag();
    void restore_debug_flag();

private:
    void base_log(std::string log_data, std::string tag);
    bool debug_flag;
    std::mutex debug_flag_mutex;
    char log_buf[SPACEX_LOG_BUF_SIZE];
    Log(void);
};

} // namespace spacex

#endif /* !_SPACEX_LOG_H_ */
