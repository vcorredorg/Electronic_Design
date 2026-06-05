#include "zephyr_compat.h"
#include "control.h"

void main(void)
{
    printk("lock-in firmware starting\n");

    int ret = control_init();
    if (ret != 0)
    {
        printk("init failed: %d\n", ret);
        return;
    }

    control_start();
}
