#ifndef LOGGING_H
#define LOGGING_H

enum LogLevel { LL_DEBUG = 0, LL_INFO = 1, LL_WARN = 2, LL_ERR = 3 };

// Initilizes IO-ports needed for logging
int init_logger();

// Sends the message to COM1. Qemu can capture all data sent to this port and output it to a file
void log(enum LogLevel level, char* msg);

#endif  // LOGGING_H
