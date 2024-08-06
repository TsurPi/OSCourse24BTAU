#include "os.h"

// Helper function to get the index for a given level
static uint64_t get_index(uint64_t vpn, int level) {
    return (vpn >> (12 + 9 * level)) & 0x1FF;  // Extract 9 bits for the level
}

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn) {
    uint64_t *current = (uint64_t*)phys_to_virt(pt << 12);
    for (int level = 4; level > 0; level--) {
        uint64_t index = get_index(vpn, level);
        if (!(current[index] & 1)) {  // If valid bit is not set
            if (ppn == NO_MAPPING) {
                return;  // Mapping does not exist, nothing to remove
            }
            current[index] = (alloc_page_frame() << 12) | 1;  // Allocate new page table and set valid bit
        }
        current = (uint64_t*)phys_to_virt(current[index] & ~0xFFF);
    }
    
    uint64_t index = get_index(vpn, 0);
    if (ppn == NO_MAPPING) {
        current[index] = 0;  // Invalidate the entry
    } else {
        current[index] = (ppn << 12) | 1;  // Set the mapping and valid bit
    }
}

uint64_t page_table_query(uint64_t pt, uint64_t vpn) {
    uint64_t *current = (uint64_t*)phys_to_virt(pt << 12);
    for (int level = 4; level > 0; level--) {
        uint64_t index = get_index(vpn, level);
        if (!(current[index] & 1)) {
            return NO_MAPPING;  // No valid mapping exists
        }
        current = (uint64_t*)phys_to_virt(current[index] & ~0xFFF);
    }
    
    uint64_t index = get_index(vpn, 0);
    if (!(current[index] & 1)) {
        return NO_MAPPING;  // No valid mapping exists
    }
    return (current[index] >> 12);  // Return the physical page number
}