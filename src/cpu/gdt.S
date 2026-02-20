.global load_and_flush_gdt

load_and_flush_gdt:
    pushq %rbp
    movq %rsp, %rbp
    
    # First argument (struct gdt_ptr *) is passed into RDI
    lgdt (%rdi)

    # Update all data segment registers
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    
    # Flush code segment
    # We can't simply store a value into CS. The best way to do this is to use 'lretq' (long return).
    # 'lretq' pops the first value (from the stack) into RIP and pops the next value into CS.
    pushq $0x08
    lea .flush_cs(%rip), %rax   # Load the address of the label below
    pushq %rax
    lretq

.flush_cs:
    # The GDT is now fully loaded and flushed
    leave
    ret

