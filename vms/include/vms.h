#ifndef VMS_H
#define VMS_H

#include <stdint.h>

#define PAGE_SIZE 4096

/* MMU */
void vms_write(void* pointer, int value);
int vms_read(void* pointer);
void* vms_get_root_page_table();
void vms_set_root_page_table(void* pointer);

/* Pages */
void vms_init();
void* vms_new_page();
void vms_free_page(void*);
int vms_get_used_pages();
void* vms_get_page_pointer(int index);
int vms_get_page_index(void* pointer);

/* Page Table */
uint16_t vms_page_table_index(void* virtual_address, int level);
uint64_t* vms_page_table_pte_entry_from_index(void* page_table, int index);
uint64_t* vms_page_table_pte_entry(void* page_table,
                                   void* virtual_address,
                                   int level);
void* vms_ppn_to_page(uint64_t ppn);
uint64_t vms_page_to_ppn(void* pointer);

/* PTE */
void vms_pte_valid_clear(uint64_t* entry);
void vms_pte_valid_set(uint64_t* entry);
int vms_pte_valid(uint64_t* entry);
void vms_pte_read_clear(uint64_t* entry);
void vms_pte_read_set(uint64_t* entry);
int vms_pte_read(uint64_t* entry);
void vms_pte_write_clear(uint64_t* entry);
void vms_pte_write_set(uint64_t* entry);
int vms_pte_write(uint64_t* entry);
void vms_pte_custom_clear(uint64_t* entry);
void vms_pte_custom_set(uint64_t* entry);
int vms_pte_custom(uint64_t* entry);
uint64_t vms_pte_get_ppn(uint64_t* entry);
void vms_pte_set_ppn(uint64_t* entry, uint64_t ppn);

/* VMS */
void* vms_fork_copy();
void* vms_fork_copy_on_write();

#endif
