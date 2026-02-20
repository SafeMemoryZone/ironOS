#include <cpu/gdt.h>
#include <debug/logging.h>

// Halt and catch fire function
static void hcf(void) {
	for (;;) {
		asm("hlt");
	}
}

// Function for disabling interrupts
static void cli() { asm("cli"); }

// Kernel entry point
void kmain(void) {
	// Disable interrupts
	cli();

	// Initilization
	if (init_logger()) {
		hcf();  // We can't panic here because the logger isn't initilized
	}
	log(LL_INFO, "Initilized logger");
	init_gdt();
	log(LL_INFO, "Initilized GDT");
	log(LL_INFO, "Kernel initilization success");

	// Stop execution
	hcf();
}
