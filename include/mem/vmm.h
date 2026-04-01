#ifndef VMM_H
#define VMM_H

#include <limine/limine.h>
#include <stdint.h>

typedef uint64_t pte_t;  // page table entry type

int map_page(pte_t *pml4, void *virt_addr, void *phys_addr, uint64_t flags);
void unmap_page(pte_t *pml4, void *virt_addr);
int init_vmm(const struct limine_memmap_response *mmap,
             const struct limine_executable_address_response *exec_addr);

#endif  // VMM_H
