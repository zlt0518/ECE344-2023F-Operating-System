#include "vms.h"

#include "mmu.h"
#include "pages.h"

#include <string.h>

static int page_pointed_num[MAX_PAGES] = {0};

void page_fault_handler(void* virtual_address, int level, void* page_table) {

    if(level>0) return;

    uint64_t* entry_curr = vms_page_table_pte_entry(page_table,virtual_address, level);

    //invalid
    if(!vms_pte_valid(entry_curr)) return;
    //not point to a multi-pointed pages
    if(!vms_pte_custom(entry_curr)) return;

    
    uint64_t ppn_curr = vms_pte_get_ppn(entry_curr);
    void* page_curr = vms_ppn_to_page(ppn_curr);
    int page_index_curr = vms_get_page_index(page_curr);

    //map new page as something is also reading the page otherwise just change the permission
    if(page_pointed_num[page_index_curr] > 0)
    {
        page_pointed_num[page_index_curr]--;

        //create and copy the new page
        void * page_new = vms_new_page();
        memcpy(page_new, page_curr, PAGE_SIZE);
    
        uint64_t ppn_new = vms_page_to_ppn(page_new);

        //old entry point to new page and make it writable 
        vms_pte_set_ppn(entry_curr, ppn_new);
        vms_pte_write_set(entry_curr);
        
        //valid
        vms_pte_valid_set(entry_curr);
    }

    //clear the entry as it will point to writable entry
    vms_pte_custom_clear(entry_curr);
    vms_pte_write_set(entry_curr);

}

//helper function for recursive call of creating page
//IsCoW: Copy-On-Write: 1 Non-Copy-On-Write: 0
void create_page(int level,void* parent_page_upper,void* child_page_upper,int IsCoW)
{
    //base case return
    if( level < 0 ) return;

    for(int i = 0; i<NUM_PTE_ENTRIES;++i)
    {   
        //get pointer to the parent entry 
        uint64_t* parent_entry_curr = vms_page_table_pte_entry_from_index(parent_page_upper,i);

        //if not valid return        
        if(!vms_pte_valid(parent_entry_curr)) continue;

        //trace to the page pointed from the pointer
        uint64_t parent_ppn = vms_pte_get_ppn(parent_entry_curr);
        void *parent_page_curr = vms_ppn_to_page(parent_ppn);

        //case for the level 0  of Copy on Write
        if(level == 0 && IsCoW == 1)
        {
            //update the array that record the number for pte that pointed toward the page
            page_pointed_num[vms_get_page_index(parent_page_curr)] ++;            

            uint64_t *child_entry_curr = vms_page_table_pte_entry_from_index(child_page_upper, i);

            //point from child to parent entry
            vms_pte_set_ppn(child_entry_curr,parent_ppn);

            //permission change for Copy On Write
            if (vms_pte_valid(parent_entry_curr)) vms_pte_valid_set(child_entry_curr);
            if (vms_pte_read (parent_entry_curr)) vms_pte_read_set(child_entry_curr); 
            if (vms_pte_custom(parent_entry_curr)) vms_pte_custom_set(child_entry_curr);

            if (vms_pte_write(parent_entry_curr)){
                vms_pte_custom_set(parent_entry_curr);
                vms_pte_custom_set(child_entry_curr);
            }

            //not writable
            vms_pte_write_clear(child_entry_curr);
            vms_pte_write_clear(parent_entry_curr);

        }
        else
        {
            void *child_page_curr = vms_new_page();
            memcpy(child_page_curr, parent_page_curr, PAGE_SIZE);

            create_page(level-1,parent_page_curr,child_page_curr,IsCoW); //recursive call  for one level down

            uint64_t *child_entry_curr = vms_page_table_pte_entry_from_index(child_page_upper, i);
            uint64_t child_ppn = vms_page_to_ppn(child_page_curr);
            vms_pte_set_ppn(child_entry_curr,child_ppn);

            // permission change
            if (vms_pte_valid(parent_entry_curr))  vms_pte_valid_set(child_entry_curr);
            if (vms_pte_read (parent_entry_curr))  vms_pte_read_set(child_entry_curr);
            if (vms_pte_custom(parent_entry_curr)) vms_pte_custom_set(child_entry_curr);
            if (vms_pte_write (parent_entry_curr)) vms_pte_write_set(child_entry_curr);
        }
    }
    return;
}

void* vms_fork_copy() {
    void * parent_page = vms_get_root_page_table();
    void * child_page = vms_new_page(); 
    memcpy(child_page,parent_page,PAGE_SIZE);

    create_page(2,parent_page,child_page,0);
    
    return child_page;
}


void* vms_fork_copy_on_write() {

    void * parent_page = vms_get_root_page_table();
    void * child_page = vms_new_page(); 
    memcpy(child_page,parent_page,PAGE_SIZE);

    create_page(2,parent_page,child_page,1);

    return child_page;
}

