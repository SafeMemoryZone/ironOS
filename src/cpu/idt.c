#include <cpu/idt.h>
#include <debug/logging.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct idt_gate_descriptor {
	uint16_t offset_low : 16;  // offset bits 0..15
	uint16_t selector : 16;    // a code segment selector in GDT or LDT
	uint8_t ist : 3;           // Interrupt Stack Table offset
	uint8_t reserved1 : 5;
	uint8_t type_attributes : 8;  // gate type, dpl, and p fields
	uint16_t offset_mid : 16;     // offset bits 16..31
	uint32_t offset_high : 32;    // offset bits 32..63
	uint32_t reserved2 : 32;
} __attribute__((packed));

struct idt_ptr {
	uint16_t size : 16;
	uint64_t offset : 64;
} __attribute__((packed));

/*
 * Type Attributes Byte Layout:
 * | P | DPL | 0 | Gate Type |
 * | 7 | 6 5 | 4 | 3 2 1 0  |
 *
 * Gate Types (64-bit long mode):
 * - 0xE (1110): Interrupt Gate — clears IF on entry, disabling further interrupts
 * - 0xF (1111): Trap Gate — IF unchanged, interrupts remain enabled
 */

#define IDT_ATTR_PRESENT (1 << 7)
#define IDT_ATTR_DPL_KERNEL (0 << 5)

#define IDT_INTERRUPT_GATE (IDT_ATTR_PRESENT | IDT_ATTR_DPL_KERNEL | 0x0E)
#define IDT_TRAP_GATE (IDT_ATTR_PRESENT | IDT_ATTR_DPL_KERNEL | 0x0F)

#define KERNEL_CS 0x08
#define EXCEPTION_COUNT 32
#define IDT_ENTRY_COUNT 256

struct idt_gate_descriptor idt_vector[IDT_ENTRY_COUNT] = {0};

// Defined in idt.S
typedef void (*isr_stub_t)(void);
extern isr_stub_t isr_stub_table[];
extern void load_idt(struct idt_ptr*);

struct interrupt_frame {
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
	uint64_t vector;
	uint64_t error_code;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

static const char* exception_names[] = {
    "Divide Error",                   // 0  #DE
    "Debug",                          // 1  #DB
    "NMI Interrupt",                  // 2  NMI
    "Breakpoint",                     // 3  #BP
    "Overflow",                       // 4  #OF
    "BOUND Range Exceeded",           // 5  #BR
    "Invalid Opcode",                 // 6  #UD
    "Device Not Available",           // 7  #NM
    "Double Fault",                   // 8  #DF
    "Coprocessor Segment Overrun",    // 9
    "Invalid TSS",                    // 10 #TS
    "Segment Not Present",            // 11 #NP
    "Stack-Segment Fault",            // 12 #SS
    "General Protection",             // 13 #GP
    "Page Fault",                     // 14 #PF
    "Reserved",                       // 15
    "x87 FPU Error",                  // 16 #MF
    "Alignment Check",                // 17 #AC
    "Machine Check",                  // 18 #MC
    "SIMD Floating-Point Exception",  // 19 #XM
    "Virtualization Exception",       // 20 #VE
    "Control Protection Exception",   // 21 #CP
};

// We can dynamically change the exception handler. For testing of the IDT we use a custom exception
// handler
void (*active_exception_handler)(struct interrupt_frame*) = NULL;

static inline struct idt_ptr make_idtr() {
	return (struct idt_ptr){.size = sizeof(idt_vector) - 1, .offset = (uint64_t)&idt_vector};
}

static inline void install_idt_descriptor(int off, isr_stub_t handler, uint8_t type_attr) {
	uint64_t offset = (uint64_t)handler;
	idt_vector[off] = (struct idt_gate_descriptor){
	    .offset_low = offset & 0xFFFF,
	    .selector = KERNEL_CS,
	    .ist = 0,
	    .reserved1 = 0,
	    .type_attributes = type_attr,
	    .offset_mid = (offset >> 16) & 0xFFFF,
	    .offset_high = (offset >> 32) & 0xFFFFFFFF,
	    .reserved2 = 0,
	};
}

static void install_exception_descriptors() {
	for (int i = 0; i < EXCEPTION_COUNT; i++) {
		uint8_t gate_type;
		switch (i) {
			case 2:   // NMI — non-maskable, must not be interrupted
			case 8:   // Double Fault — critical, prevent further nesting
			case 18:  // Machine Check — fatal, prevent further nesting
				gate_type = IDT_INTERRUPT_GATE;
				break;
			default:  // All other exceptions use trap gates
				gate_type = IDT_TRAP_GATE;
				break;
		}
		install_idt_descriptor(i, isr_stub_table[i], gate_type);
	}
}

static void exception_handler(struct interrupt_frame* frame) {
	if (frame->vector < sizeof(exception_names) / sizeof(exception_names[0])) {
		log(LL_ERR, (char*)exception_names[frame->vector]);
	}
	else {
		log(LL_ERR, "Unknown exception");
	}

	for (;;) {
		asm("hlt");
	}
}

void init_idt() {
	active_exception_handler = exception_handler;  // install default exception handler

	install_exception_descriptors();  // only installs descriptors for CPU exceptions

	struct idt_ptr idtr = make_idtr();
	load_idt(&idtr);
}

// === DEBUG ===

static int last_vector;

static void testing_exception_handler(struct interrupt_frame* frame) {
	last_vector = frame->vector;
}

// Only tests IDT vector #0-#31
bool test_exceptions() {
	// Install test exception handler
	active_exception_handler = testing_exception_handler;

#define ASSERT_LAST_IDT_VECTOR_NUM(num) \
	if (last_vector != num) return false

// For vectors without error codes: use int N directly
#define TRIGGER_INTERRUPT(n) asm volatile("int %0" : : "i"(n))

// For vectors with error codes: int N never pushes an error code, but isr_err stubs
// expect one. We build a fake interrupt frame with a dummy error code on the stack and
// jump directly to the ISR stub
#define _STR(x) #x
#define STR(x) _STR(x)
#define KERNEL_SS 0x10
#define TRIGGER_INTERRUPT_ERR(n) \
	asm volatile(     \
    "movq %%rsp, %%rdx\n\t"                        \
    "pushq $" STR(KERNEL_SS) "\n\t"  /* fake SS */  \
    "pushq %%rdx\n\t"               /* fake RSP */  \
    "pushfq\n\t"                     /* RFLAGS */    \
    "pushq $" STR(KERNEL_CS) "\n\t"  /* fake CS */   \
    "leaq 1f(%%rip), %%rax\n\t"                      \
    "pushq %%rax\n\t"               /* fake RIP */   \
    "pushq $0\n\t"              /* dummy error code */\
    "jmp isr_stub_" #n "\n\t"                         \
    "1:\n\t"                                           \
    : : : "rax", "rdx", "memory")

	TRIGGER_INTERRUPT(0);
	ASSERT_LAST_IDT_VECTOR_NUM(0);
	TRIGGER_INTERRUPT(1);
	ASSERT_LAST_IDT_VECTOR_NUM(1);
	TRIGGER_INTERRUPT(2);
	ASSERT_LAST_IDT_VECTOR_NUM(2);
	TRIGGER_INTERRUPT(3);
	ASSERT_LAST_IDT_VECTOR_NUM(3);
	TRIGGER_INTERRUPT(4);
	ASSERT_LAST_IDT_VECTOR_NUM(4);
	TRIGGER_INTERRUPT(5);
	ASSERT_LAST_IDT_VECTOR_NUM(5);
	TRIGGER_INTERRUPT(6);
	ASSERT_LAST_IDT_VECTOR_NUM(6);
	TRIGGER_INTERRUPT(7);
	ASSERT_LAST_IDT_VECTOR_NUM(7);
	TRIGGER_INTERRUPT_ERR(8);
	ASSERT_LAST_IDT_VECTOR_NUM(8);
	TRIGGER_INTERRUPT(9);
	ASSERT_LAST_IDT_VECTOR_NUM(9);
	TRIGGER_INTERRUPT_ERR(10);
	ASSERT_LAST_IDT_VECTOR_NUM(10);
	TRIGGER_INTERRUPT_ERR(11);
	ASSERT_LAST_IDT_VECTOR_NUM(11);
	TRIGGER_INTERRUPT_ERR(12);
	ASSERT_LAST_IDT_VECTOR_NUM(12);
	TRIGGER_INTERRUPT_ERR(13);
	ASSERT_LAST_IDT_VECTOR_NUM(13);
	TRIGGER_INTERRUPT_ERR(14);
	ASSERT_LAST_IDT_VECTOR_NUM(14);
	TRIGGER_INTERRUPT(15);
	ASSERT_LAST_IDT_VECTOR_NUM(15);
	TRIGGER_INTERRUPT(16);
	ASSERT_LAST_IDT_VECTOR_NUM(16);
	TRIGGER_INTERRUPT_ERR(17);
	ASSERT_LAST_IDT_VECTOR_NUM(17);
	TRIGGER_INTERRUPT(18);
	ASSERT_LAST_IDT_VECTOR_NUM(18);
	TRIGGER_INTERRUPT(19);
	ASSERT_LAST_IDT_VECTOR_NUM(19);
	TRIGGER_INTERRUPT(20);
	ASSERT_LAST_IDT_VECTOR_NUM(20);
	TRIGGER_INTERRUPT_ERR(21);
	ASSERT_LAST_IDT_VECTOR_NUM(21);
	TRIGGER_INTERRUPT(22);
	ASSERT_LAST_IDT_VECTOR_NUM(22);
	TRIGGER_INTERRUPT(23);
	ASSERT_LAST_IDT_VECTOR_NUM(23);
	TRIGGER_INTERRUPT(24);
	ASSERT_LAST_IDT_VECTOR_NUM(24);
	TRIGGER_INTERRUPT(25);
	ASSERT_LAST_IDT_VECTOR_NUM(25);
	TRIGGER_INTERRUPT(26);
	ASSERT_LAST_IDT_VECTOR_NUM(26);
	TRIGGER_INTERRUPT(27);
	ASSERT_LAST_IDT_VECTOR_NUM(27);
	TRIGGER_INTERRUPT(28);
	ASSERT_LAST_IDT_VECTOR_NUM(28);
	TRIGGER_INTERRUPT(29);
	ASSERT_LAST_IDT_VECTOR_NUM(29);
	TRIGGER_INTERRUPT(30);
	ASSERT_LAST_IDT_VECTOR_NUM(30);
	TRIGGER_INTERRUPT(31);
	ASSERT_LAST_IDT_VECTOR_NUM(31);

#undef ASSERT_LAST_IDT_VECTOR_NUM
#undef TRIGGER_INTERRUPT
#undef TRIGGER_INTERRUPT_ERR
#undef _STR
#undef STR
#undef KERNEL_SS

	// Remove test exception handler (and install default one)
	active_exception_handler = exception_handler;

	return true;
}
