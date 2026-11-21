#ifndef LOG_H
#define LOG_H

void log_init(const char *path);
void log_msg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
const char *log_path(void);

#endif
