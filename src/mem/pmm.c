#include <builtins.h>
#include <limine/limine.h>
#include <mem/pmm.h>
#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096

typedef uint64_t bucket_t;

struct bitmap {
	bucket_t* base;
	size_t length;
	size_t hint_index;
};

extern uintptr_t hhdm;

struct bitmap pmm_bitmap = {.base = NULL, .length = 0, .hint_index = 0};

// divides and rounds up to the nearest integer
static inline uint64_t div_ceil(uint64_t a, uint64_t b) { return (a + b - 1) / b; }

int pmm_init(const struct limine_memmap_response* mmap) {
	// Step 1: walk the memory map and store:
	// 1. largest usable region
	// 2. highest memory address
	struct limine_memmap_entry* largest_usable_region = NULL;
	uintptr_t highest_addr = 0;

	for (uint64_t i = 0; i < mmap->entry_count; i++) {
		struct limine_memmap_entry* region = mmap->entries[i];

		uintptr_t end = region->base + region->length - 1;
		if (end > highest_addr) {
			highest_addr = end;
		}

		if (region->type == LIMINE_MEMMAP_USABLE) {
			if (largest_usable_region == NULL) {
				largest_usable_region = region;
			}
			else if (region->length > largest_usable_region->length) {
				largest_usable_region = region;
			}
		}
	}

	// Step 2: allocate a bitmap for tracking free pages
	// NOTE: Limine guarantees that usable and bootloader reclaimable entries are 4096-byte aligned
	// in both base and length
	uint64_t page_count = div_ceil(
	    highest_addr, PAGE_SIZE);  // how many pages are needed to represent the whole address space

	// the bitmap is separated into buckets (each 64 bits -> 64 pages)
	// the length represents the bucket count. NOT the size in bytes
	uintptr_t bitmap_phys = largest_usable_region->base;
	bucket_t* bitmap_virt = (bucket_t*)(bitmap_phys + hhdm);
	uint64_t bitmap_length = div_ceil(page_count, sizeof(bucket_t) * 8);
	uint64_t bitmap_size = bitmap_length * sizeof(bucket_t);

	if (bitmap_phys + bitmap_size > largest_usable_region->base + largest_usable_region->length) {
		return 1;  // not enough space
	}

	// set all pages to used
	memset(bitmap_virt, 255, bitmap_size);

	pmm_bitmap.base = bitmap_virt;
	pmm_bitmap.length = bitmap_length;

	// Step 3: free pages that are usable
	for (uint64_t i = 0; i < mmap->entry_count; i++) {
		struct limine_memmap_entry* region = mmap->entries[i];

		if (region->type != LIMINE_MEMMAP_USABLE) {
			continue;
		}

		uintptr_t region_start = region->base;
		uintptr_t region_end = region->base + region->length;

		for (uintptr_t addr = region_start; addr < region_end; addr += PAGE_SIZE) {
			// don't free pages occupied by the bitmap itself
			// this check relies on the fact that the bitmap is page-aligned
			if (addr >= bitmap_phys && addr < bitmap_phys + bitmap_size) {
				continue;
			}

			uint64_t page = addr / PAGE_SIZE;
			pmm_bitmap.base[page / 64] &= ~((bucket_t)1 << (page % 64));
		}
	}

	return 0;
}

uintptr_t pmm_alloc() {
	size_t bucket_idx = pmm_bitmap.hint_index;

	for (size_t buckets_visited = 0; buckets_visited != pmm_bitmap.length; buckets_visited++) {
		bucket_t bucket = pmm_bitmap.base[bucket_idx];

		// we found a free page
		if (bucket != UINT64_MAX) {
			int page_idx = __builtin_ctzll(~bucket);

			pmm_bitmap.base[bucket_idx] |= 1ULL << page_idx;
			pmm_bitmap.hint_index = bucket_idx;

			uintptr_t page_base_addr = (sizeof(bucket_t) * 8 * bucket_idx + page_idx) * PAGE_SIZE;
			return page_base_addr;
		}
		bucket_idx = (bucket_idx + 1) % pmm_bitmap.length;
	}

	return UINT64_MAX;
}
