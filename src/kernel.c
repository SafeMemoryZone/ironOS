#include <cpu/gdt.h>
#include <cpu/idt.h>
#include <debug/logging.h>
#include <limine/limine.h>
#include <mem/pmm.h>
#include <mem/vmm.h>
#include <pmio/pic.h>

// Limine base revision and request markers
__attribute__((used, section(".limine_requests"))) static volatile uint64_t limine_base_revision[] =
    LIMINE_BASE_REVISION(6);

__attribute__((
    used, section(".limine_requests_start"))) static volatile uint64_t limine_requests_start[] =
    LIMINE_REQUESTS_START_MARKER;

__attribute__((used,
               section(".limine_requests_end"))) static volatile uint64_t limine_requests_end[] =
    LIMINE_REQUESTS_END_MARKER;

// Limine requests
__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0};

__attribute__((
    used, section(".limine_requests"))) static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID, .revision = 0};

__attribute__((
    used, section(".limine_requests"))) static volatile struct limine_executable_address_request
    exec_addr_request = {.id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID, .revision = 0};

// Halt and catch fire function
static void hcf(void) {
	for (;;) {
		asm("hlt");
	}
}

// Function for disabling interrupts
static void cli() { asm("cli"); }

uintptr_t hhdm;

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
	init_idt();
	log(LL_INFO, "Initilized IDT");
	init_pic();
	log(LL_INFO, "Initilized PIC");

	if (!memmap_request.response || !hhdm_request.response) {
		log(LL_ERR, "Failed to get memory map or HHDM from Limine");
		hcf();
	}
	hhdm = hhdm_request.response->offset;

	if (pmm_init(memmap_request.response)) {
		log(LL_ERR, "Failed to initilize PMM");
		hcf();
	}
	log(LL_INFO, "Initilized PMM");

	if (!exec_addr_request.response) {
		log(LL_ERR, "Failed to get executable address from Limine");
		hcf();
	}
	if (init_vmm(memmap_request.response, exec_addr_request.response)) {
		log(LL_ERR, "Failed to initilize VMM");
		hcf();
	}
	log(LL_INFO, "Initilized VMM");

	log(LL_INFO, "Kernel initilization succeeded");

	// Stop execution
	hcf();
}
