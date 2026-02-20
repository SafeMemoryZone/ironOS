#include <cpu/gdt.h>
#include <stdint.h>

/*
 * Global Descriptor Table (GDT) Entry Definitions
 * * Documentation: https://wiki.osdev.org/Global_Descriptor_Table
 * * Access Byte Layout:
 * | P | DPL | S | E | DC | RW | A |
 * | 7 | 6 5 | 4 | 3 | 2  | 1  | 0 |
 */

// Definitions for access byte
#define GDT_ACCESS_PRESENT (1 << 7)
#define GDT_ACCESS_DPL(x) ((x) << 5)
#define GDT_ACCESS_DPL_KERNEL (0 << 5)
#define GDT_ACCESS_DPL_USER (3 << 5)
#define GDT_ACCESS_S_SYSTEM (0 << 4)
#define GDT_ACCESS_S_CODE_DATA (1 << 4)
#define GDT_ACCESS_E_DATA (0 << 3)
#define GDT_ACCESS_E_CODE (1 << 3)
#define GDT_ACCESS_DC_GROWS_UP (0 << 2)
#define GDT_ACCESS_DC_GROWS_DOWN (1 << 2)
#define GDT_ACCESS_DC_NON_CONFORM (0 << 2)
#define GDT_ACCESS_DC_CONFORMING (1 << 2)
#define GDT_ACCESS_RW_READ_ONLY (0 << 1)
#define GDT_ACCESS_RW_WRITABLE (1 << 1)
#define GDT_ACCESS_RW_EXEC_ONLY (0 << 1)
#define GDT_ACCESS_RW_READABLE (1 << 1)
#define GDT_ACCESS_ACCESSED (1 << 0)

/*
 * Flags Layout:
 * | G | D/B | L | AVL |
 * | 3 |  2  | 1 |  0  |
 */

// Definitions for flags
#define GDT_FLAG_GRANULARITY_BYTE (0 << 3)
#define GDT_FLAG_GRANULARITY_4K (1 << 3)
#define GDT_FLAG_SIZE_16BIT (0 << 2)
#define GDT_FLAG_SIZE_32BIT (1 << 2)
#define GDT_FLAG_LONG_MODE_OFF (0 << 1)
#define GDT_FLAG_LONG_MODE_64BIT (1 << 1)
#define GDT_FLAG_RESERVED (0 << 0)

struct gdt_segment_descriptor {
	uint16_t limit_low : 16;  // ignored in 64-bit mode
	uint16_t base_low : 16;   // ignored in 64-bit mode
	uint8_t base_mid : 8;     // ignored in 64-bit mode
	uint8_t access_byte : 8;  // stores permissions
	uint8_t limit_high : 4;   // ignored in 64-bit mode
	uint8_t flags : 4;        // stores additional segement info
	uint8_t base_high : 8;    // ignored in 64-bit mode
} __attribute__((packed));

struct gdt_system_descriptor_high {
	uint32_t base : 32;  // higher bits of the base address
	uint32_t reserved : 32;
} __attribute__((packed));

struct gdt_ptr {
	uint16_t size : 16;
	uint64_t addr : 64;
} __attribute__((packed));

// We must define a type that's compatible with gdt_segment_descriptor and
// gdt_system_descriptor_high
typedef uint64_t gdt_entry_t;

// 6 entries for null, kernel code, kernel data, user code and user data
#define GDT_ENTRY_COUNT 6

// Limine will initilize the .bss section for us
static gdt_entry_t GDT[GDT_ENTRY_COUNT];

// Defined in gdt.S
extern void load_and_flush_gdt(struct gdt_ptr*);

static inline void install_segment_descriptor(int off, const void* desc) {
	GDT[off] = *(gdt_entry_t*)desc;
}

static inline struct gdt_ptr make_gdtr() {
	return (struct gdt_ptr){.size = sizeof(GDT) - 1, .addr = (uint64_t)&GDT};
}

void init_gdt() {
	struct gdt_segment_descriptor null = {0}, kernel_code = {0}, kernel_data = {0}, user_code = {0},
	                              user_data = {0};

	// TODO: Implement TSS

	// Kernel code
	kernel_code.access_byte =
	    GDT_ACCESS_ACCESSED          // best practice to set this
	    | GDT_ACCESS_RW_READABLE     // read access is allowed
	    | GDT_ACCESS_DC_NON_CONFORM  // code can only be executed by ring 0
	    | GDT_ACCESS_E_CODE          // defines a code segement
	    | GDT_ACCESS_S_CODE_DATA     // defines a code or data segment (not a system segment)
	    | GDT_ACCESS_DPL_KERNEL      // ring 0 (kernel)
	    | GDT_ACCESS_PRESENT         // must be present for any valid entry
	    ;
	kernel_code.flags = GDT_FLAG_LONG_MODE_64BIT;  // Defines a 64-bit code segment. All other flags
	                                               // are ignored in long mode

	// Kernel data
	kernel_data.access_byte =
	    GDT_ACCESS_ACCESSED       // best practice to set this
	    | GDT_ACCESS_RW_WRITABLE  // write access is allowed
	    | GDT_ACCESS_DC_GROWS_UP  // data grows up
	    | GDT_ACCESS_E_DATA       // defines a data segement
	    | GDT_ACCESS_S_CODE_DATA  // defines a code or data segment (not a system segment)
	    | GDT_ACCESS_DPL_KERNEL   // ring 0 (kernel)
	    | GDT_ACCESS_PRESENT      // must be present for any valid entry
	    ;
	kernel_data.flags = 0;  // Flags must be 0 here (L must be 0 for data segments). All other flags
	                        // are ignored in long mode

	// User code
	user_code.access_byte =
	    GDT_ACCESS_ACCESSED          // best practice to set this
	    | GDT_ACCESS_RW_READABLE     // read access is allowed
	    | GDT_ACCESS_DC_NON_CONFORM  // code can only be executed by ring 3
	    | GDT_ACCESS_E_CODE          // defines a code segement
	    | GDT_ACCESS_S_CODE_DATA     // defines a code or data segment (not a system segment)
	    | GDT_ACCESS_DPL_USER        // ring 3 (kernel)
	    | GDT_ACCESS_PRESENT         // must be present for any valid entry
	    ;
	user_code.flags = GDT_FLAG_LONG_MODE_64BIT;  // Defines a 64-bit code segment. All other flags
	                                             // are ignored in long mode

	// User data
	user_data.access_byte =
	    GDT_ACCESS_ACCESSED       // best practice to set this
	    | GDT_ACCESS_RW_WRITABLE  // write access is allowed
	    | GDT_ACCESS_DC_GROWS_UP  // data grows up
	    | GDT_ACCESS_E_DATA       // defines a data segement
	    | GDT_ACCESS_S_CODE_DATA  // defines a code or data segment (not a system segment)
	    | GDT_ACCESS_DPL_USER     // ring 0 (kernel)
	    | GDT_ACCESS_PRESENT      // must be present for any valid entry
	    ;
	user_data.flags = 0;  // Flags must be 0 here (L must be 0 for data segments). All other flags
	                      // are ignored in long mode

	// WARN: Each GDT segment descriptor MUST be installed into specific offsets. This is due to the
	// SYSCALL and SYSRET instructions, that assume a specific structure. load_and_flush_gdt also
	// relies on specific offsets.
	install_segment_descriptor(0, &null);
	install_segment_descriptor(1, &kernel_code);
	install_segment_descriptor(2, &kernel_data);
	install_segment_descriptor(3, &null);  // 32-bit user code. Left on null for now
	install_segment_descriptor(4, &user_data);
	install_segment_descriptor(5, &user_code);

	// Load the GDT
	struct gdt_ptr gdtr = make_gdtr();
	load_and_flush_gdt(&gdtr);
}
