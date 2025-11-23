#include <kernel/panic.h>
#include <kernel/printk.h>
#include <mm/stack.h>

uint64_t __stack_chk_guard = STACK_CANARY_VALUE;

void __stack_chk_fail(void) { panic("Stack overflow detected!"); }

void stack_check_init(void) {
    __stack_chk_guard = STACK_CANARY_VALUE;
    printk("Stack: initialized with fixed canary\n");
}