#include <kernel/pid.h>
#include <kernel/spinlock.h>
#include <string.h>

static uint32_t pid_bitmap[MAX_PID / 32];
static spinlock_t pid_lock = SPINLOCK_INIT;

void pid_init(void) {
    memset(pid_bitmap, 0, sizeof(pid_bitmap));
    pid_bitmap[0] |= 1;
}

int pid_alloc(void) {
    spinlock_lock(&pid_lock);

    for (int i = 1; i < MAX_PID; i++) {
        int word = i / 32;
        int bit = i % 32;
        if (!(pid_bitmap[word] & (1U << bit))) {
            pid_bitmap[word] |= (1U << bit);
            spinlock_unlock(&pid_lock);
            return i;
        }
    }

    spinlock_unlock(&pid_lock);
    return -1;
}

void pid_free(int pid) {
    if (pid <= 0 || pid >= MAX_PID)
        return;

    spinlock_lock(&pid_lock);
    int word = pid / 32;
    int bit = pid % 32;
    pid_bitmap[word] &= ~(1U << bit);
    spinlock_unlock(&pid_lock);
}
