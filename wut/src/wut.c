#include "wut.h"

#include <assert.h> // assert
#include <errno.h> // errno
#include <stddef.h> // NULL
#include <stdio.h> // perror
#include <stdlib.h> // reallocarray
#include <sys/mman.h> // mmap, munmap
#include <sys/signal.h> // SIGSTKSZ
#include <sys/queue.h> // TAILQ_*
#include <ucontext.h> // getcontext, makecontext, setcontext, swapcontext
#include <valgrind/valgrind.h> // VALGRIND_STACK_REGISTER


struct wut_thread_tcb
{

    int occupied; //0 for unused and 1 for in use/used -> check for available thread number

    ucontext_t* thread_ucontext;

    int thread_status; //status code -> 0 / 128 / exit code
    int done; //the thread has finished -> set in clean-up stage
    int main_thread; //the first thread

    int joining_thread; //thread waiting for
    int joined_thread;  //some thread that called join on itself wait for it to cancel

};

static int thread_list_num; 
static int thread_num;
struct wut_thread_tcb *wut_thread_list; //ptr to thread array

//linked list from lecture
struct list_entry 
{
    int thread_id;
    TAILQ_ENTRY(list_entry) pointers;
};

//head of linked list
TAILQ_HEAD(list_head, list_entry);
static struct list_head list_head;


static void die(const char* message) 
{
    int err = errno;
    perror(message);
    exit(err);
}

static char* new_stack(void) 
{
    char* stack = mmap(
        NULL,
        SIGSTKSZ,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_ANONYMOUS | MAP_PRIVATE,
        -1,
        0
    );
    if (stack == MAP_FAILED) 
    {
        die("mmap stack failed");
    }
    VALGRIND_STACK_REGISTER(stack, stack + SIGSTKSZ);
    return stack;
}

static void delete_stack(char* stack) 
{
    if (munmap(stack, SIGSTKSZ) == -1) 
    {
        die("munmap stack failed");
    }
}

void free_thread_stack(int id)
{
    //only free if its not the first thread
    if (wut_thread_list[id].main_thread == 0) 
    {
        delete_stack(wut_thread_list[id].thread_ucontext->uc_stack.ss_sp);
        free(wut_thread_list[id].thread_ucontext);
    }
}


void clean_up_work()
{   
    int current_thread_id = wut_id();   

    //remove the thread from queue and decrease the number
    struct list_entry* current_thread = TAILQ_FIRST(&list_head);
    TAILQ_REMOVE(&list_head, current_thread, pointers);

    //free the stack if all things are done
    free_thread_stack(current_thread_id);

    //write done signal
    wut_thread_list[current_thread_id].done = 1;
    thread_num--;

    if (thread_num == 0)
    {
        //exit for no thread now
        exit(0);
    }

    //check if any thread is waiting for the current thread to finish
    if (wut_thread_list[current_thread_id].joined_thread != -1){
        
        //make it not wait and add back to the queue
        wut_thread_list[wut_thread_list[current_thread_id].joined_thread].joining_thread = -1;

        // add the calling thread back to the end of the queue
        struct list_entry* new_entry = malloc(sizeof(struct list_entry));
        new_entry->thread_id = wut_thread_list[current_thread_id].joined_thread;
        TAILQ_INSERT_TAIL(&list_head, new_entry, pointers);

    }    

    //get thread if the list is not empty and context switch
    if (!TAILQ_EMPTY(&list_head))
    {
        struct list_entry* next_thread = TAILQ_FIRST(&list_head);
        int next_thread_id = next_thread->thread_id;
        setcontext(wut_thread_list[next_thread_id].thread_ucontext);
    }

}


void wut_init() 
{
    //initialize a list
    TAILQ_INIT(&list_head);
    assert(TAILQ_EMPTY(&list_head));

    //set up the thread list with the first thread allocated
    thread_list_num = 1;
    thread_num = 1;
    wut_thread_list = malloc(thread_list_num * sizeof(struct wut_thread_tcb));

    wut_thread_list[0].occupied = 1;
    wut_thread_list[0].done = 0;
    wut_thread_list[0].main_thread = 1;
    wut_thread_list[0].joined_thread = -1;
    wut_thread_list[0].joining_thread = -1;
    wut_thread_list[0].thread_status = 0;

    //set up context with no links to the uc context
    ucontext_t* ucontext_first = malloc(sizeof(ucontext_t));
    getcontext(ucontext_first);
    ucontext_first->uc_link = NULL;

    wut_thread_list[0].thread_ucontext = ucontext_first;

    // add tcb to queue
    struct list_entry* entry_curr = malloc(sizeof(struct list_entry));
    entry_curr->thread_id = 0;
    TAILQ_INSERT_TAIL(&list_head, entry_curr, pointers);
}

int wut_id() 
{
    return TAILQ_FIRST(&list_head)->thread_id;
}

int wut_find_thread_id()
{
    int i = 0;

    //find the empty entry in the thread list
    while(i<thread_list_num)
    {
        if(wut_thread_list[i].occupied == 0)
        {   
            return i;
        }

        i++;
    }

    //no enough place for the thread, all occupied-> allocate longer array
    int new_thread_id = thread_list_num;
    thread_list_num++;
    wut_thread_list = realloc(wut_thread_list, thread_list_num * sizeof(struct wut_thread_tcb)) ;
    return new_thread_id;
    
}

int wut_create(void (*run)(void))
{

    int new_thread_id = wut_find_thread_id();

    wut_thread_list[new_thread_id].occupied = 1;
    wut_thread_list[new_thread_id].joined_thread = -1;
    wut_thread_list[new_thread_id].joining_thread = -1;
    wut_thread_list[new_thread_id].done = 0;
    wut_thread_list[new_thread_id].main_thread = 0;
    wut_thread_list[new_thread_id].thread_status = 0;

    //end context
    ucontext_t* finish_context = malloc(sizeof(ucontext_t));
    getcontext(finish_context);
    finish_context -> uc_stack.ss_sp = new_stack();  
    finish_context -> uc_stack.ss_size = SIGSTKSZ;
    makecontext(finish_context, clean_up_work, 0);


    // making a new context for thread_id
    ucontext_t* new_thread_ucontext = malloc(sizeof(ucontext_t));
    getcontext(new_thread_ucontext);
    new_thread_ucontext->uc_stack.ss_sp = new_stack();
    new_thread_ucontext->uc_stack.ss_size = SIGSTKSZ;

    // linking the new context to the clean up context
    new_thread_ucontext->uc_link = finish_context;
    makecontext(new_thread_ucontext, run, 0);

    wut_thread_list[new_thread_id].thread_ucontext = new_thread_ucontext;

    // add tcb to queue
    struct list_entry* entry_curr = malloc(sizeof(struct list_entry));
    entry_curr->thread_id = new_thread_id;
    TAILQ_INSERT_TAIL(&list_head, entry_curr, pointers);    
    
    //increase thread num
    thread_num++;

    return new_thread_id;
}

int wut_cancel(int id) {
    
    if (thread_num <= 0)
    {
        return -1;
    }

    //check valid
    int current_thread_id = wut_id();
    if(id==current_thread_id || id<0 || id>thread_num) return -1;
    if(wut_thread_list[id].occupied == 0) return -1;
    if(wut_thread_list[id].done == 1) return -1;
    
    //decrease count
    thread_num-=1;

    //remove from list if possible
    struct list_entry* entry_current;
    TAILQ_FOREACH(entry_current, &list_head, pointers) 
    {
        if (entry_current->thread_id == id)
        {      
            // if in queue, remove it
            TAILQ_REMOVE(&list_head, entry_current, pointers);
            break;
        }
    }
    
    //change status
    wut_thread_list[id].thread_status = 128;
    
    //free stack
    free_thread_stack(id);

    if(wut_thread_list[id].joining_thread != -1)
    {
        //if the cancelling thread is joining someone, it will just inform the joined thread no one is waiting for it to 
        //finishes
        wut_thread_list[wut_thread_list[id].joining_thread].joined_thread = -1;
        wut_thread_list[id].joining_thread = -1;
    }

    if(wut_thread_list[id].joined_thread != -1)
    {
        //smth for the thread that is waiting
        wut_thread_list[wut_thread_list[id].joined_thread].joining_thread = -1;
        

        //thread that is waiting for this thread to finish
        struct list_entry* new_entry = malloc(sizeof(struct list_entry));

        new_entry->thread_id = wut_thread_list[id].joined_thread;
        wut_thread_list[id].joined_thread = -1;

        //add back to the queue
        TAILQ_INSERT_TAIL(&list_head, new_entry, pointers);     
    }

    return 0;
}

int wut_join(int id) {
    
    if(thread_num <= 0||thread_num>thread_list_num) return -1;

    int current_thread_id = wut_id();

    //check valid
    if(id==current_thread_id || id<0 || id>thread_num) return -1;
    if(wut_thread_list[id].occupied == 0) return -1;

    //some one is waiting for the thread or itself is waiting for some other thread
    if(wut_thread_list[id].joined_thread!=-1 ||wut_thread_list[current_thread_id].joining_thread!=-1) return -1;

    //if it is already cancelled of exit
    if(wut_thread_list[id].thread_status == 128)
    {
        //reset and free the finished thread
        wut_thread_list[id].occupied = 0;

        return wut_thread_list[id].thread_status;
    }

    //already finished naturally
    if(wut_thread_list[id].done == 1)
    {
        //reset and free the finished thread
        wut_thread_list[id].occupied = 0;

        return wut_thread_list[id].thread_status;
    }

    //change joinid
    wut_thread_list[id].joined_thread = current_thread_id;
    wut_thread_list[current_thread_id].joining_thread = id;

    //remove first thread that calls join from the queue
    struct list_entry* current_thread = TAILQ_FIRST(&list_head);
    current_thread_id = current_thread->thread_id;
    TAILQ_REMOVE(&list_head, current_thread, pointers);
    
    //make to next thread
    struct list_entry* next_thread = TAILQ_FIRST(&list_head);
    int next_thread_id = next_thread->thread_id;

    //swap context 
    swapcontext(wut_thread_list[current_thread_id].thread_ucontext, wut_thread_list[next_thread_id].thread_ucontext);

    //return back to the context where the target thread has ended and the current thread has been added back and currently running
    //reset and free the finished thread
    int ret_status = wut_thread_list[id].thread_status;
    wut_thread_list[id].occupied = 0;

    return ret_status;
}

int wut_yield() {
   
    if(thread_num<2)
    {
        return -1;
    }

    struct list_entry* current_thread = TAILQ_FIRST(&list_head);
    int current_thread_id = current_thread->thread_id;

    //remove and insert at back
    TAILQ_REMOVE(&list_head, current_thread, pointers);
    TAILQ_INSERT_TAIL(&list_head, current_thread, pointers);
    
    struct list_entry* next_thread = TAILQ_FIRST(&list_head);
    int next_thread_id = next_thread->thread_id;
    
    //register swap
    swapcontext(wut_thread_list[current_thread_id].thread_ucontext, wut_thread_list[next_thread_id].thread_ucontext);
    return 0;    

}


void wut_exit(int status) {

    int current_thread_id = wut_id();

    status &= 0xFF;
    wut_thread_list[current_thread_id].thread_status = status;

    if(thread_num == 1){
        // final process
        exit(0);
    }
    //for the first thread that is not linked to cleanning process
    if(wut_thread_list[current_thread_id].main_thread == 1)
    {
        ucontext_t* finish_context = malloc(sizeof(ucontext_t));
        getcontext(finish_context);
        finish_context -> uc_stack.ss_sp = new_stack();  
        finish_context -> uc_stack.ss_size = SIGSTKSZ;
        makecontext(finish_context, clean_up_work, 0);
        setcontext(finish_context);

    }
    else
    {
        //set to the the cleaning process of the thread
        setcontext((wut_thread_list[current_thread_id].thread_ucontext)->uc_link);
    }

}
