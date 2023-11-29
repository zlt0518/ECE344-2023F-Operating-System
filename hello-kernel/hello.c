#include <linux/module.h>
#include <linux/printk.h>

int hello_init(void) {
    pr_info("Hello kernel.\n");
    return 0;
}

void hello_exit(void) {
    pr_info("Goodbye kernel.\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_AUTHOR("Jon Eyolfson:)");
MODULE_LICENSE("ECE344F");
