#include <builtins.h>
#include <debug/logging.h>
#include <limine/limine.h>
#include <mem/pmm.h>
#include <mem/vmm.h>
#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096

// Basic Access & Privilege Flags
#define PAGE_PRESENT (1ULL << 0)  // P: Page is in memory
#define PAGE_RW (1ULL << 1)       // R/W: Read/Write permission (0 = RO, 1 = RW)
#define PAGE_USER (1ULL << 2)     // U/S: User/Supervisor (0 = Kernel only, 1 = User space)

// Caching Flags
#define PAGE_PWT (1ULL << 3)         // PWT: Page-level Write-Through
#define PAGE_PCD (1ULL << 4)         // PCD: Page-level Cache Disable
#define PAGE_PAT_PT (1ULL << 7)      // PAT: Page Attribute Table (for 4 KiB Page Table entries)
#define PAGE_PAT_LARGE (1ULL << 12)  // PAT: Page Attribute Table (for 2 MiB / 1 GiB large pages)

// CPU Managed Accounting Flags
#define PAGE_ACCESSED (1ULL << 5)  // A: Set by CPU when page is read/written
#define PAGE_DIRTY (1ULL << 6)     // D: Set by CPU when page is written to

// Size & TLB Flags
#define PAGE_SIZE_LARGE \
	(1ULL << 7)  // PS: Page Size (Points to 2MiB or 1GiB page instead of next table)
#define PAGE_GLOBAL (1ULL << 8)  // G: Global page (TLB is not flushed on CR3 switch)

// Security Flags
#define PAGE_NX (1ULL << 63)  // XD/NX: Execute Disable (Prevents code execution from this page)

// Bitmask to extract the physical address from a 64-bit entry
// (Assuming standard 48-bit physical addressing, masks out the lower 12 bits and upper 12 bits)
#define PAGE_PHYS_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define PML4_GET_INDEX(va) (((uint64_t)(va) >> 39) & 0x1FF)
#define PDPT_GET_INDEX(va) (((uint64_t)(va) >> 30) & 0x1FF)
#define PD_GET_INDEX(va) (((uint64_t)(va) >> 21) & 0x1FF)
#define PT_GET_INDEX(va) (((uint64_t)(va) >> 12) & 0x1FF)

extern uintptr_t hhdm;

static pte_t *kernel_pml4;          // kernel page table (virtual address via HHDM)
static uintptr_t kernel_pml4_phys;  // physical address for CR3

// Linker-provided section boundary symbols
extern char _kernel_start[];
extern char _kernel_end[];
extern char _text_start[];
extern char _text_end[];
extern char _rodata_start[];
extern char _rodata_end[];
extern char _data_start[];
extern char _data_end[];

static inline void *phys_to_virt(uintptr_t phys) { return (void *)(phys + hhdm); }

// Allocate a zeroed page table. Returns physical address or UINT64_MAX on failure
static uintptr_t alloc_table(void) {
	uintptr_t phys = pmm_alloc();
	if (phys == UINT64_MAX) {
		return UINT64_MAX;
	}
	memset(phys_to_virt(phys), 0, PAGE_SIZE);
	return phys;
}

static inline void load_cr3(uintptr_t pml4_phys) {
	__asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}

static inline void invlpg(uintptr_t vaddr) {
	__asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

int map_page(pte_t *pml4, void *virt_addr, void *phys_addr, uint64_t flags) {
	size_t page_table_indexes[] = {PML4_GET_INDEX(virt_addr), PDPT_GET_INDEX(virt_addr),
	                               PD_GET_INDEX(virt_addr), PT_GET_INDEX(virt_addr)};

	pte_t *curr_table = pml4;

	// Walk/create PML4 -> PDPT -> PD (first 3 levels)
	// Intermediate entries need PRESENT | RW (and USER if the final mapping needs it)
	uint64_t int_flags = PAGE_PRESENT | PAGE_RW;
	if (flags & PAGE_USER) {
		int_flags |= PAGE_USER;
	}

	for (size_t i = 0; i < 3; i++) {
		pte_t entry = curr_table[page_table_indexes[i]];

		if (!(entry & PAGE_PRESENT)) {
			uintptr_t new_table = alloc_table();
			if (new_table == UINT64_MAX) {
				return 1;
			}
			curr_table[page_table_indexes[i]] = new_table | int_flags;
			entry = curr_table[page_table_indexes[i]];
		}

		curr_table = (pte_t *)phys_to_virt(entry & PAGE_PHYS_ADDR_MASK);
	}

	// Set the final PT entry
	curr_table[page_table_indexes[3]] = ((uintptr_t)phys_addr & PAGE_PHYS_ADDR_MASK) | flags;
	return 0;
}

void unmap_page(pte_t *pml4, void *virt_addr) {
	pte_t pml4e = pml4[PML4_GET_INDEX(virt_addr)];
	if (!(pml4e & PAGE_PRESENT)) return;

	pte_t *pdpt = (pte_t *)phys_to_virt(pml4e & PAGE_PHYS_ADDR_MASK);
	pte_t pdpte = pdpt[PDPT_GET_INDEX(virt_addr)];
	if (!(pdpte & PAGE_PRESENT)) return;

	pte_t *pd = (pte_t *)phys_to_virt(pdpte & PAGE_PHYS_ADDR_MASK);
	pte_t pde = pd[PD_GET_INDEX(virt_addr)];
	if (!(pde & PAGE_PRESENT)) return;

	pte_t *pt = (pte_t *)phys_to_virt(pde & PAGE_PHYS_ADDR_MASK);
	pt[PT_GET_INDEX(virt_addr)] = 0;

	invlpg((uintptr_t)virt_addr);
}

static void map_range(pte_t *pml4, uintptr_t virt_start, uintptr_t virt_end, uintptr_t phys_start,
                      uint64_t flags) {
	for (uintptr_t off = 0; virt_start + off < virt_end; off += PAGE_SIZE) {
		map_page(pml4, (void *)(virt_start + off), (void *)(phys_start + off), flags);
	}
}

int init_vmm(const struct limine_memmap_response *mmap,
             const struct limine_executable_address_response *exec_addr) {
	// Step 1: allocate PML4
	kernel_pml4_phys = alloc_table();
	if (kernel_pml4_phys == UINT64_MAX) {
		return 1;
	}
	kernel_pml4 = (pte_t *)phys_to_virt(kernel_pml4_phys);

	// Step 2: map the HHDM
	// Every memory region gets mapped at hhdm + phys_addr
	for (uint64_t i = 0; i < mmap->entry_count; i++) {
		struct limine_memmap_entry *entry = mmap->entries[i];

		uintptr_t base = entry->base & ~(uintptr_t)(PAGE_SIZE - 1);
		uintptr_t end = (entry->base + entry->length + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);

		map_range(kernel_pml4, base + hhdm, end + hhdm, base, PAGE_PRESENT | PAGE_RW | PAGE_NX);
	}

	// Step 3: map kernel sections with correct permissions (W^X enforced)
	uintptr_t virt_base = exec_addr->virtual_base;
	uintptr_t phys_base = exec_addr->physical_base;

#define KPHYS(vaddr) (phys_base + ((uintptr_t)(vaddr) - virt_base))

	// .limine_requests: read-only, no-execute
	map_range(kernel_pml4, (uintptr_t)_kernel_start, (uintptr_t)_text_start, KPHYS(_kernel_start),
	          PAGE_PRESENT | PAGE_NX);

	// .text: read-only, executable
	map_range(kernel_pml4, (uintptr_t)_text_start, (uintptr_t)_text_end, KPHYS(_text_start),
	          PAGE_PRESENT);

	// .rodata: read-only, no-execute
	map_range(kernel_pml4, (uintptr_t)_rodata_start, (uintptr_t)_rodata_end, KPHYS(_rodata_start),
	          PAGE_PRESENT | PAGE_NX);

	// .data + .bss: read-write, no-execute
	map_range(kernel_pml4, (uintptr_t)_data_start, (uintptr_t)_data_end, KPHYS(_data_start),
	          PAGE_PRESENT | PAGE_RW | PAGE_NX);

#undef KPHYS

	// Step 4: switch to the new page tables
	load_cr3(kernel_pml4_phys);

	return 0;
}
