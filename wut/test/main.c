#include "wut.h"

#include <stdio.h>
#include <unistd.h>

/* Do not modify this function, you should call this to check for any value
   you want to inspect from the solution. */
void check(int value, const char* message) {
    printf("Check: %d (%s)\n", value, message);
}

void run1(void) {
    return;
}

int main() {
    /*
    You may write any of your own testing code here.

    You can execute it using `build/test/wut`. However, without your own
    implementation, it shouldn't do much. We'll run this code with the solution
    so you can clarify anything you want, or write a tricky test case.

    Place at least one call to `check` with a value (probably a return from a
    library call) that you'd like to see the output of. For example, here's
    how to convert `tests/main-thread-is-0.c` into this format:
    
    wut_init();
    check(wut_id(), "wut_id of the main thread is should be 0");

    */
    int shared_memory[2];
    wut_init();

    shared_memory[0] = wut_id();
    int id1 = wut_create(run1); // Before: {0} -> After: {0, 1}
    shared_memory[1] = id1;

    check(shared_memory[0], "wut_id of the main thread is should be 0");
    check(shared_memory[0], "wut_id of the main thread is should be 1");



    return 0;
}
