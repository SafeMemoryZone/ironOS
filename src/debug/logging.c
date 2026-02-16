#include "debug/logging.h"

#include <pmio/com1.h>
#include <stddef.h>

int init_logger() { return init_com1(); }

static void write_str(const char* str) {
	while (*str) {
		write_com1(*str);
		str++;
	}
}

void log(enum LogLevel level, char* msg) {
	if (msg == NULL) {
		return;
	}

	const char* LL_NAMES[] = {"DEBUG", "INFO", "WARN", "ERROR"};

	// Format: [TIME] [LEVEL]: message

	// TODO: print time

	// Log level
	write_str(LL_NAMES[level]);
	write_str(": ");
	write_str(msg);
	// Newline after log
	write_com1('\n');
}
