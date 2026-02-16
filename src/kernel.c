#include <debug/logging.h>

// Halt and catch fire function.
static void hcf(void) {
	for (;;) {
		asm("hlt");
	}
}

// Kernel entry point
void kmain(void) {
	// Initilize logger before anything else
	init_logger();

	log(LL_INFO, "Kernel gained control");

	// Halt
	hcf();
}
